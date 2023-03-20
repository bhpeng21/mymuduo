#pragma once

#include <vector>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <any> //什么用的？

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"
#include "Logger.h"
#include "TimerId.h"
#include "Callbacks.h"

namespace mymuduo
{
class TimerQueue;
class Poller;
class Channel;

// 事件循环类，主要包含了两大模块  Channel 与Poller（还有定时器事件）
class EventLoop : public noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    //在当前loop中执行cb
    void runInLoop(Functor cb);
    // 把cb放入队列，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

    // 通过eventfd唤醒loop所在的线程
    void wakeup();

    //EventLoop的方法 => Poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    //这里少了一个assertInLoopThread();
    //还少了一个static EventLoop* getEventLoopOfCurrentThread();
    // 判断EventLoop对象是否在自己的线程里边
    bool isInLoopThread() const 
    { return threadId_ == CurrentThread::tid(); } // threadId_为EventLoop创建时的线程id CurrentThread::tid()为当前线程id

    bool eventHandling() const { return eventHandling_; }

    void setContext(const std::any& context)
    { context_ = context; }

    const std::any& getContext() const
    { return context_; }

    std::any* getMutableContext()
    { return &context_; }

    /*timers*/
    TimerId runAt(Timestamp time, TimerCallback cb);
    TimerId runAfter(double delay, TimerCallback cb);
    TimerId runEvery(double interval, TimerCallback cb);
    void cancel(TimerId timerId);

private:
    using ChannelList = std::vector<Channel *>;

    //下面这个函数为什么两个项目都不要了？
    // void abortNotInLoopThread();
    void handleRead();        // 给eventfd返回的文件描述符wakeupFd_绑定的事件回调 当wakeup()时 即有事件发生时 调用handleRead()读wakeupFd_的8字节 同时唤醒阻塞的epoll_wait
    void doPendingFunctors(); // 执行上层回调

    //c++11项目为什么不要这个？   好像硕哥使用这个来debug的。
    void printActiveChannels() const;

    std::atomic_bool looping_;  //原子操作，通过CAS实现的
    std::atomic_bool quit_; // 标志退出loop循环
    std::atomic_bool eventHandling_;    // 表示是否在处理事件

    const pid_t threadId_; // 记录当前EventLoop是被哪个线程id创建的 即标识了当前EventLoop的所属线程id
    Timestamp pollReturnTime_; // Poller返回发生事件的Channels的时间点

    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;
    int wakeupFd_; // 作用：当mainLoop获取一个新用户的Channel 需通过轮询算法选择一个subLoop 通过该成员唤醒subLoop处理Channel
    std::unique_ptr<Channel> wakeupChannel_;

    std::any context_;

    ChannelList activeChannels_;    // 返回Poller检测到当前有事件发生的所有Channel列表
    Channel* currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;    // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                        // 互斥锁 用来保护上面vector容器的线程安全操作

};
}