#pragma once

#include <functional>

#include "Channel.h"
#include "Socket.h"

namespace mymuduo
{
namespace net
{

class EventLoop;
class InetAddress;

// Acceptor of incoming TCP connections.
class Acceptor : public noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    { newConnectionCallback_ = cb; }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
    int idleFd_;
};

}  // namespace net
}  // namespace mymuduo
