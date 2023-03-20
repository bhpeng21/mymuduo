#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

namespace mymuduo
{

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if(sockfd < 0)
    {
        LOG_FATAL("Acceptor createNonblocking  sockfd error! errno=%d", errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reusePort)
    : loop_(loop),
      acceptSocket_(createNonblocking()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    // TcpServer::start()  Acceptor::listen  有新用户的连接，需要执行一个回调(accept=>connfd=>callback)
    //(然后callback中可能会:1.把connfd注册成Channel对象——单线程服务器；2.把connfd移交subLoop——多线程服务器)
    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();    //把注册到Poller中的事件注销
    acceptChannel_.remove();        //channel调用EventLoop::removeChannel()=>Poller::removeChannel()
    ::close(idleFd_);
    // acceptSocket RAII 自己会析构
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();         //listen
    acceptChannel_.enableReading(); //acceptChannel_注册到Poller
}

//listenfd有事件发生了 ，即新用户连接
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr);   //轮询找到subloop，唤醒，分发当前fd的channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("Acceptor accept error\n");
        //设一个空的fd占位，当fd资源都满了后，就释放这个空fd，把来的lfd接受再立马关闭，然后再接着占位
        if(errno == EMFILE)
        {
            ::close(idleFd_);
            ::accept(idleFd_, nullptr, nullptr);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}

}