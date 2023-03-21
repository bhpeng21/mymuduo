#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <any>

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

namespace mymuduo
{

class Channel;
class EventLoop;
class Socket;

/*
TcpServer => Acceptor => 有一个新用户连接，通过accept拿到connfd
 => TcpConnection设置回调 => Channel => Poller => Channel的回调操作
*/
class TcpConnection : public noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

    TcpConnection(EventLoop *loop, 
                  const std::string &name,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    //用户用的发送数据接口
    void send(const std::string&);
    //用户用的关闭连接接口
    void shutdown();
    void setTcpNoDelay(bool on);

    void forceClose();
    void forceCloseInLoop();

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }
    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }

    //连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

    void setState(StateE state) { state_ = state; }

    void setContext(const std::any& context)
    { context_ = context; }
    const std::any& getContext() const
    { return context_; }

    std::any* getMutableContext()
    { return &context_; }
private:
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_; // 这里是baseloop还是subloop由TcpServer中创建的线程数决定 若为多Reactor 该loop_指向subloop 若为单Reactor 该loop_指向baseloop
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // Socket Channel 这里和Acceptor类似    Acceptor => mainloop    TcpConnection => subloop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 这些回调TcpServer也有 用户通过写入TcpServer注册 TcpServer再将注册的回调传递给TcpConnection TcpConnection再将回调注册到Channel中
    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;    //接受数据的缓冲区
    Buffer outputBuffer_;   //发送数据的缓冲区
    std::any context_;
};
}