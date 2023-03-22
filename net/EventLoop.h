#pragma once

#include <any>
#include <atomic>
#include <functional>
#include <vector>

#include "../base/Mutex.h"
#include "../base/CurrentThread.h"
#include "../base/Timestamp.h"
#include "Callbacks.h"
#include "TimerId.h"

namespace mymuduo
{
namespace net
{

class Channel;
class Poller;
class TimerQueue;

/// Reactor, at most one per thread.
/// This is an interface class, so don't expose too much details.
// 事件循环类，主要包含了两大模块  Channel 与Poller（还有定时器事件）
class EventLoop : noncopyable
{
public:
    typedef std::function<void()> Functor;

    EventLoop();
    ~EventLoop(); // force out-line dtor, for std::unique_ptr members.

    // Loops forever.
    // Must be called in the same thread as creation of the object.
    //开启事件循环
    void loop();

    // Quits loop.
    // This is not 100% thread safe, if you call through a raw pointer,
    // better to call through shared_ptr<EventLoop> for 100% safety.
    //退出事件循环
    void quit();

    /// Time when poll returns, usually means data arrival.
    Timestamp pollReturnTime() const { return pollReturnTime_; }

    int64_t iteration() const { return iteration_; }

    // Runs callback immediately in the loop thread.
    // It wakes up the loop, and run the cb.
    // If in the same loop thread, cb is run within the function.
    // Safe to call from other threads.
    //在当前loop中执行cb
    void runInLoop(Functor cb);
    // Queues callback in the loop thread.
    // Runs after finish pooling.
    // Safe to call from other threads.
    // 把cb放入队列，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

    size_t queueSize() const;

    // timers

    // Runs callback at 'time'.
    // Safe to call from other threads.
    TimerId runAt(Timestamp time, TimerCallback cb);
    // Runs callback after @c delay seconds.
    // Safe to call from other threads.
    TimerId runAfter(double delay, TimerCallback cb);
    // Runs callback every @c interval seconds.
    // Safe to call from other threads.
    TimerId runEvery(double interval, TimerCallback cb);
    // Cancels the timer.
    // Safe to call from other threads.
    void cancel(TimerId timerId);

    // internal usage
    // 通过eventfd唤醒loop所在的线程
    void wakeup();
    //EventLoop的方法 => Poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // pid_t threadId() const { return threadId_; }
    // 判断EventLoop对象是否在自己的线程里边
    void assertInLoopThread()
    {
        if (!isInLoopThread())
        {
            abortNotInLoopThread();
        }
    }
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    // bool callingPendingFunctors() const { return callingPendingFunctors_; }
    bool eventHandling() const { return eventHandling_; }

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

    static EventLoop *getEventLoopOfCurrentThread();

private:
    void abortNotInLoopThread();
    // 给eventfd返回的文件描述符wakeupFd_绑定的事件回调 当wakeup()时 即有事件发生时 调用handleRead()读wakeupFd_的8字节 同时唤醒阻塞的epoll_wait
    void handleRead(); // waked up
    // 执行上层回调
    void doPendingFunctors();

    void printActiveChannels() const; // DEBUG

    typedef std::vector<Channel *> ChannelList;

    bool looping_; /* atomic */                 // 原子操作，通过CAS实现的
    std::atomic<bool> quit_;                    // 标志退出loop循环
    bool eventHandling_;          /* atomic */  // 表示是否在处理事件
    bool callingPendingFunctors_; /* atomic */  // 标识当前loop是否有需要执行的回调操作
    int64_t iteration_;
    const pid_t threadId_;                      // 记录当前EventLoop是被哪个线程id创建的 即标识了当前EventLoop的所属线程id
    Timestamp pollReturnTime_;                  // Poller返回发生事件的Channels的时间点
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;
    int wakeupFd_;                              // 作用：当mainLoop获取一个新用户的Channel 需通过轮询算法选择一个subLoop 通过该成员唤醒subLoop处理Channel
    // unlike in TimerQueue, which is an internal class,
    // we don't expose Channel to client.
    std::unique_ptr<Channel> wakeupChannel_;
    std::any context_;

    // scratch variables
    ChannelList activeChannels_;                // 返回Poller检测到当前有事件发生的所有Channel列表
    Channel *currentActiveChannel_;

    mutable MutexLock mutex_;                                   // 互斥锁 用来保护下面vector容器的线程安全操作
    std::vector<Functor> pendingFunctors_ GUARDED_BY(mutex_);   // 存储loop需要执行的所有回调操作
};

}  // namespace net
}  // namespace mymuduo