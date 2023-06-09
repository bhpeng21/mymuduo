#pragma once

#include "../base/noncopyable.h"
#include "../base/Timestamp.h"

#include <functional>
#include <memory>

namespace mymuduo
{
namespace net
{

class EventLoop;    //前置声明类，在.cc中再包含，这样用户使用此.h的没有那么多包含

/*
    理解好EventLoop、Channel、Poller之间的关系      Reactor模型对应多路事件分发器
    Channel封装了sockfd以及这个sockfd感兴趣的event（events_），比如EPOLLIN、EPOLLOUT事件
        还绑定了Poller返回的具体事件（revent_）
*/
class Channel : public noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到Poller通知以后，该函数会在EventLoop::loop()中调用，根据revent_的值
    void handleEvent(Timestamp receiveTime);
    
    // 设置回调函数对象
    // 调function的移动赋值函数
    void setReadCallback(ReadEventCallback cb)
    {
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb)
    {
        writeCallback_ = std::move(cb);
    }
    void setCloseCallback(EventCallback cb)
    {
        closeCallback_ = std::move(cb);
    }
    void setErrorCallback(EventCallback cb)
    {
        errorCallback_ = std::move(cb);
    }

    // 防止当channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; } // used by pollers
    // int revents() const { return revents_; }
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    // 设置fd相应的事件状态
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的事件状态
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // for Poller
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // for debug
    string reventsToString() const;
    string eventsToString() const;

    void doNotLogHup() { logHup_ = false; }

    // one loop per thread  返回所属的loop
    EventLoop *ownerLoop() { return loop_; }
    void remove();

private:
    static string eventsToString(int fd, int ev);

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;       //事件循环
    const int fd_;          // fd, Poller监听的对象
    int events_;            // 注册fd感兴趣的事件
    int revents_;           // poller返回的事件
    int index_;             // used by Poller.
    bool logHup_;

    std::weak_ptr<void> tie_;   // removeChannel时使用
    bool tied_;
    bool eventHandling_;    //标志着是否正在处理事件，防止析构一个正在处理事件的Channel
    bool addedToLoop_;      // 标志着Channel是否在Loop的ChannelLists，也即是否被添加
    // 因为channel通道里面能获知fd的事件revent，所以要具体注册回调事件
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

}  // namespace net
}  // namespace mymuduo
