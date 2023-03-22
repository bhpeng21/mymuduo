#pragma once

#include "../base/noncopyable.h"
#include "../base/StringPiece.h"
#include "../base/Types.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "InetAddress.h"

#include <any>
#include <memory>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace mymuduo
{
namespace net
{

class Channel;
class EventLoop;
class Socket;

/// TCP connection, for both client and server usage.
/// This is an interface class, so don't expose too much details.
/*
TcpServer => Acceptor => 有一个新用户连接，通过accept拿到connfd
 => TcpConnection设置回调 => Channel => Poller => Channel的回调操作
*/
class TcpConnection : public noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
public:
    // Constructs a TcpConnection with a connected sockfd
    // User should not create this object.
    TcpConnection(EventLoop *loop,
                  const string &name,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }
    const string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }
    // return true if success.
    bool getTcpInfo(struct tcp_info *) const;
    string getTcpInfoString() const;

    // void send(string&& message); // C++11
    //用户用的发送数据接口
    void send(const void *message, int len);
    void send(const StringPiece &message);
    // void send(Buffer&& message); // C++11
    void send(Buffer *message); // this one will swap data
    //用户用的关闭连接接口
    void shutdown();            // NOT thread safe, no simultaneous calling
    // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
    void forceClose();
    void forceCloseWithDelay(double seconds);
    void setTcpNoDelay(bool on);
    // reading or not
    void startRead();
    void stopRead();
    bool isReading() const { return reading_; }; // NOT thread safe, may race with start/stopReadInLoop

    void setContext(const std::any &context)
    {
        context_ = context;
    }

    const std::any &getContext() const
    {
        return context_;
    }

    std::any *getMutableContext()
    {
        return &context_;
    }

    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    /// Advanced interface
    Buffer *inputBuffer()
    {
        return &inputBuffer_;
    }

    Buffer *outputBuffer()
    {
        return &outputBuffer_;
    }

    /// Internal use only.
    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }

    // called when TcpServer accepts a new connection
    //连接建立
    void connectEstablished(); // should be called only once
    // called when TcpServer has removed me from its map
    // 连接销毁
    void connectDestroyed(); // should be called only once

private:
    enum StateE
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    // void sendInLoop(string&& message);
    void sendInLoop(const StringPiece &message);
    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();
    // void shutdownAndForceCloseInLoop(double seconds);
    void forceCloseInLoop();
    void setState(StateE s) { state_ = s; }
    const char *stateToString() const;
    void startReadInLoop();
    void stopReadInLoop();

    EventLoop *loop_;   // 这里是baseloop还是subloop由TcpServer中创建的线程数决定 若为多Reactor 该loop_指向subloop 若为单Reactor 该loop_指向baseloop
    const string name_;
    StateE state_; // FIXME: use atomic variable
    bool reading_;
    // we don't expose those classes to client.
    // Socket Channel 这里和Acceptor类似    Acceptor => mainloop    TcpConnection => subloop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    // 这些回调TcpServer也有 用户通过写入TcpServer注册 TcpServer再将注册的回调传递给TcpConnection TcpConnection再将回调注册到Channel中
    ConnectionCallback connectionCallback_;         // 有新连接时的回调
    MessageCallback messageCallback_;               // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;   // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;
    Buffer inputBuffer_;    //接受数据的缓冲区
    Buffer outputBuffer_;   //发送数据的缓冲区
    std::any context_;
    // FIXME: creationTime_, lastReceiveTime_
    //        bytesReceived_, bytesSent_
};

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

}  // namespace net
}  // namespace mymuduo