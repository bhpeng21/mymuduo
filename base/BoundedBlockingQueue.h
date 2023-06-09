#pragma once

#include "Condition.h"
#include "Mutex.h"

#include <deque>
#include <assert.h>

namespace mymuduo
{

template <typename T>
class BoundedBlockingQueue : noncopyable
{
public:
    explicit BoundedBlockingQueue(unsigned maxSize)
        : maxSize_(maxSize),
            mutex_(),
            notEmpty_(mutex_),
            notFull_(mutex_)
    {
    }

    void put(const T &x)
    {
        MutexLockGuard lock(mutex_);
        while (queue_.size() == maxSize_)
        {
            notFull_.wait();
        }
        assert(queue_.size() < maxSize_);
        queue_.push_back(x);
        notEmpty_.notify();
    }

    T take()
    {
        MutexLockGuard lock(mutex_);
        while (queue_.empty())
        {
            notEmpty_.wait();
        }
        assert(!queue_.empty());
        T front(queue_.front());
        queue_.pop_front();
        notFull_.notify();
        return front;
    }

    bool empty() const
    {
        MutexLockGuard lock(mutex_);
        return queue_.empty();
    }

    bool full() const
    {
        MutexLockGuard lock(mutex_);
        return queue_.size() == maxSize_;
    }

    size_t size() const
    {
        MutexLockGuard lock(mutex_);
        return queue_.size();
    }

    size_t capacity() const
    {
        MutexLockGuard lock(mutex_);
        return queue_.capacity();
    }

private:
    const unsigned maxSize_;
    mutable MutexLock mutex_;
    Condition notEmpty_ GUARDED_BY(mutex_);
    Condition notFull_ GUARDED_BY(mutex_);
    std::deque<T> queue_ GUARDED_BY(mutex_);
};

}  // namespace mymuduo