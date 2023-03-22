#pragma once

#include "../base/noncopyable.h"
#include "../base/Types.h"

#include <functional>
#include <memory>
#include <vector>

namespace mymuduo
{

namespace net
{

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : public noncopyable
{
public:
    typedef std::function<void(EventLoop *)> ThreadInitCallback;

    EventLoopThreadPool(EventLoop *baseLoop, const string &nameArg);
    ~EventLoopThreadPool();
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // valid after calling start()
    /// round-robin
    // 在多线程中，baseLoop会默认以轮询的方式分配channel给subloop
    EventLoop *getNextLoop();

    /// with the same hash code, it will always return the same EventLoop
    //使用hash的方式分配loop 
    EventLoop *getLoopForHash(size_t hashCode);

    std::vector<EventLoop *> getAllLoops();

    bool started() const
    {
        return started_;
    }

    const string &name() const
    {
        return name_;
    }

private:
    EventLoop *baseLoop_;   // 用户使用muduo创建的loop 如果线程数为1 那直接使用用户创建的loop 否则创建多EventLoop
    string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> loops_;
};

}  // namespace net
}  // namespace mymuduo