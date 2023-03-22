#pragma once

#include "../base/Condition.h"
#include "../base/Mutex.h"
#include "../base/Thread.h"

namespace mymuduo
{
namespace net
{

class EventLoop;

class EventLoopThread : public noncopyable
{
public:
    typedef std::function<void(EventLoop *)> ThreadInitCallback;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const string &name = string());
    ~EventLoopThread();
    EventLoop *startLoop();

private:
    void threadFunc();

    EventLoop *loop_ GUARDED_BY(mutex_);
    bool exiting_;
    Thread thread_;
    MutexLock mutex_;                       //互斥锁
    Condition cond_ GUARDED_BY(mutex_);     //条件变量   这两个同步变量是针对loop_数据成员的
    ThreadInitCallback callback_;
};

}  // namespace net
}  // namespace mymuduo