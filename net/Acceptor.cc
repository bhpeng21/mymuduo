#include "Acceptor.h"

#include "../base/Logging.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "SocketsOps.h"

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <unistd.h>

using namespace mymuduo;
using namespace mymuduo::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    assert(idleFd_ >= 0);
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    // TcpServer::start()  Acceptor::listen  有新用户的连接，需要执行一个回调(accept=>connfd=>callback)
    //(然后callback中可能会:1.把connfd注册成Channel对象——单线程服务器；2.把connfd移交subLoop——多线程服务器)
    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();      //把注册到Poller中的事件注销
    acceptChannel_.remove();          //channel调用EventLoop::removeChannel()=>Poller::removeChannel()
    ::close(idleFd_);
    // acceptSocket RAII 自己会析构
}

void Acceptor::listen()
{
    loop_->assertInLoopThread();
    listenning_ = true;
    acceptSocket_.listen();           //listen
    acceptChannel_.enableReading();   //acceptChannel_注册到Poller
}

//listenfd有事件发生了 ，即新用户连接
void Acceptor::handleRead()
{
    loop_->assertInLoopThread();
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        // string hostport = peerAddr.toIpPort();
        // LOG_TRACE << "Accepts of " << hostport;
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr);     //轮询找到subloop，唤醒，分发当前fd的channel
        }
        else
        {
            sockets::close(connfd);
        }
    }
    else
    {
        LOG_SYSERR << "in Acceptor::handleRead";
        //设一个空的fd占位，当fd资源都满了后，就释放这个空fd，把来的lfd接受再立马关闭，然后再接着占位
        if (errno == EMFILE)
        {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}

