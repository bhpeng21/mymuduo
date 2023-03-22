#pragma once

#include "../base/Atomic.h"
#include "../base/Types.h"
#include "TcpConnection.h"

#include <map>

namespace mymuduo
{
namespace net
{

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

// TCP server, supports single-threaded and thread-pool models.
// This is an interface class, so don't expose too much details.
// 对外使用的TcpServer
class TcpServer : public noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    // TcpServer(EventLoop* loop, const InetAddress& listenAddr);
    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const string &nameArg,
              Option option = kNoReusePort);
    ~TcpServer(); // force out-line dtor, for std::unique_ptr members.

    const string &ipPort() const { return ipPort_; }
    const string &name() const { return name_; }
    EventLoop *getLoop() const { return loop_; }

    /// Set the number of threads for handling input.
    ///
    /// Always accepts new connection in loop's thread.
    /// Must be called before @c start
    /// @param numThreads
    /// - 0 means all I/O in loop's thread, no thread will created.
    ///   this is the default value.
    /// - 1 means all I/O in another thread.
    /// - N means a thread pool with N threads, new connections
    ///   are assigned on a round-robin basis.
    //设置底层subloop的个数
    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback &cb)
    {
        threadInitCallback_ = cb;
    }
    /// valid after calling start()
    std::shared_ptr<EventLoopThreadPool> threadPool()
    {
        return threadPool_;
    }

    // Starts the server if it's not listenning.
    // It's harmless to call it multiple times.
    /// Thread safe.
    //开启服务器监听
    void start();

    /// Set connection callback.
    /// Not thread safe.
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    /// Set message callback.
    /// Not thread safe.
    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }

private:
    // Not thread safe, but in loop
    void newConnection(int sockfd, const InetAddress &peerAddr);
    // Thread safe.
    void removeConnection(const TcpConnectionPtr &conn);
    // Not thread safe, but in loop
    // 在loop中移除，不会发生多线程的错误
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    typedef std::map<string, TcpConnectionPtr> ConnectionMap;

    EventLoop *loop_; // the acceptor loop  用户定义的baseloop
    const string ipPort_;
    const string name_;
    std::unique_ptr<Acceptor> acceptor_; // //运行在mainloop， 任务是监听连接事件
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    ConnectionCallback connectionCallback_;         //有新连接时的回调
    MessageCallback messageCallback_;               //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;   //消息发送完成以后的回调
    ThreadInitCallback threadInitCallback_;         //线程初始化回调
    AtomicInt32 started_;
    // always in loop thread
    int nextConnId_;
    ConnectionMap connections_; //保存所有的连接
};

}  // namespace net
}  // namespace mymuduo