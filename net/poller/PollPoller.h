#pragma once

#include "../Poller.h"

#include <vector>

struct pollfd;

namespace mymuduo
{
namespace net
{

///
/// IO Multiplexing with poll(2).
///
class PollPoller : public Poller
{
public:
    PollPoller(EventLoop *loop);
    ~PollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    void fillActiveChannels(int numEvents,
                            ChannelList *activeChannels) const;

    typedef std::vector<struct pollfd> PollFdList;
    PollFdList pollfds_;
};

}  // namespace net
}  // namespace mymuduo