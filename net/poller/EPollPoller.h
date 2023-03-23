#pragma once

#include "../Poller.h"

#include <vector>

struct epoll_event;

namespace mymuduo
{
namespace net
{

// IO Multiplexing with epoll(4).
/*
epoll的使用
epoll_create   ==> 构造函数
epoll_ctl    add/mod/del  ==> update/remove
epoll_wait  ==> poll
*/
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    // Eventlist的初始长度
    static const int kInitEventListSize = 16;

    static const char *operationToString(int op);

    // 填写活跃的连接
    void fillActiveChannels(int numEvents,
                            ChannelList *activeChannels) const;
    // 更新channel通道 其实就是调用epoll_ctl
    void update(int operation, Channel *channel);

    using EventList = std::vector<struct epoll_event>; // C++中可以省略struct 直接写epoll_event即可

    int epollfd_;          // epoll_create创建返回的fd保存在epollfd_中
    EventList events_;     // 用于存放epoll_wait返回的所有发生的事件的文件描述符事件集
};

}  // namespace net
}  // namespace mymuduo