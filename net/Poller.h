#pragma once

#include <map>
#include <vector>

#include "../base/Timestamp.h"
#include "EventLoop.h"

namespace mymuduo
{
namespace net
{

class Channel;

// Base class for IO Multiplexing
// This class doesn't own the Channel objects.
// muduo库中多路事件分发器的核心IO复用模块
class Poller : public noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller();

    /// Polls the I/O events.
    /// Must be called in the loop thread.
    // 给所有IO复用（epoll/select）保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;

    /// Changes the interested I/O events.
    /// Must be called in the loop thread.
    virtual void updateChannel(Channel *channel) = 0;

    /// Remove the channel, when it destructs.
    /// Must be called in the loop thread.
    virtual void removeChannel(Channel *channel) = 0;

    //判断channel是否在当前Poller当中
    virtual bool hasChannel(Channel *channel) const;

    // Eventloop可以通过该接口获取默认的IO复用的具体对象
    static Poller *newDefaultPoller(EventLoop *loop);

    void assertInLoopThread() const
    {
        ownerLoop_->assertInLoopThread();
    }

protected:
    // map的key表示sockfd， value是fd所对应的channel对象
    typedef std::map<int, Channel *> ChannelMap;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_;
};

}  // namespace net
}  // namespace mymuduo