#include <sys/errno.h>
#include <fcntl.h>
#include <stdio.h>  // snprintf
#include <sys/socket.h>
#include <sys/uio.h>  // readv
#include <unistd.h>
#include <sys/types.h>
#include <cstring>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <string>
#include <functional>

#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "util.h"

namespace mymuduo
{

TcpConnection::TcpConnection(EventLoop *loop, 
                const std::string &name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(name),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)), 
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64*1024*1024)  //64M
{
    // 下面给channel设置相应的回调函数 poller给channel通知感兴趣的事件发生了 channel会回调相应的回调函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d , state=%d\n", 
                name_.c_str(), channel_->fd(), (int)state_);
}

// 读是相对服务器而言的 当对端客户端有数据到达 服务器端检测到EPOLLIN 就会触发该fd上的回调 handleRead取读走对端发来的数据
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if(n > 0)   // 有数据到达
    {
        // 已建立连接的用户有可读事件发生了 调用用户传入的回调操作onMessage shared_from_this就是获取了TcpConnection的智能指针
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if(n == 0) // 客户端断开
    {
        handleClose();
    }
    else    // 出错了
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead\n");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if(n > 0)
        {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    // 唤醒loop_对应的thread线程，执行回调
                    // TcpConnection对象在其所在的subloop中 向pendingFunctors_中加入回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                // 如果用户调了shutdown时候，还有未发送完的数据，那么就在这里发送完后进行关闭
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();   // 在当前所属的loop中把TcpConnection删除掉
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite\n");
        }
    }
    else
    {
        LOG_ERROR("Connection fd=%d is down, no more writing \n", channel_->fd());
    }
}

//poller => channel::closeCallback=>TcpConnection::handleClose=>TcpServer::removeConnection
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d \n", channel_->fd());
    setState(kDisconnected);
    channel_->disableAll();

    //回调中要在map中删除conn了，这里必须再加一个引用计数，以免conn在回调中已经无了
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   // 执行连接关闭的回调
    //必须在最后一行
    closeCallback_(connPtr);    // 关闭连接的回调(TcpServer::removeConnection)
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else 
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

void TcpConnection::send(const std::string& buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())     // 这种是对于单个reactor的情况 用户调用conn->send时 loop_即为当前线程
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop, this,
                buf.c_str(), buf.size()
            ));
        }
    }
}

/*
发送数据  应用写的快， 而内核发送数据慢， 需要把待发送数据放入缓冲区，设置水位回调
*/
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    // 之前调用过该connection的shutdown， 不能再发送了
    if(state_ == kDisconnected)
    {
        LOG_ERROR("Disconnected, give up writing!");
        return;
    }
    //表示channel第一次开始写数据，而且缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0)
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                // 既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else      //nwrote < 0
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop\n");
                if(errno == EPIPE || errno == ECONNRESET)   // SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }
    /**
     * 说明当前这一次write并没有把数据全部发送出去 剩余的数据需要保存到缓冲区当中
     * 然后给channel注册EPOLLOUT事件，Poller发现tcp的发送缓冲区有空间后会通知
     * 相应的sock->channel，调用channel对应注册的writeCallback_回调方法，
     * channel的writeCallback_实际上就是TcpConnection设置的handleWrite回调，
     * 把发送缓冲区outputBuffer_的内容全部发送完成
     **/
    if(!faultError && remaining > 0)    
    {
        // 目前发送缓冲区剩余的待发送的数据的长度
        ssize_t oldLen = outputBuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));   
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if(!channel_->isWriting())
        {
            channel_->enableWriting();  //这里一定要注册channel的写事件，否则poller不会通知Epollout
        }
    }
}

//连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向Poller注册channel的读事件

    // 新连接建立，执行用户给的回调
    connectionCallback_(shared_from_this());
}
// 连接销毁(关闭连接的最后一步)
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把channel的所感兴趣的事件del掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::shutdown()
{
    if(state_ == kDisconnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}

void TcpConnection::forceClose()
{
  // FIXME: use compare and swap
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    setState(kDisconnecting);
    loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
  }
}

void TcpConnection::forceCloseInLoop()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    // as if we received 0 byte in handleRead();
    handleClose();
  }
}

void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())  // 说明当前outputBuffer中的数据已经全部发送完成
    {
        //关闭后channel会触发closeCallback(因为有EPOLLHUP)
        socket_->shutdownWrite();   // 关闭写端
    }
}

}