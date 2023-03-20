#pragma once

#include "noncopyable.h"

namespace mymuduo
{
class InetAddress;

//封装socket fd
class Socket : public noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {}
    ~Socket();

    int fd() const { return sockfd_; }
    //bool getTcpnfo(struct tcp_info*) const;
    //bool getTcpInfoString(char* buf, int len) const;
    void bindAddress(const InetAddress& localAddr);
    void listen();
    int accept(InetAddress *peerAddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};
}