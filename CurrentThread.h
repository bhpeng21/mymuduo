#pragma once

#include <unistd.h>
#include <sys/syscall.h>


//疑问：所以这个CurrentThread在哪里呢？谁拥有这个类对象？难道是线程吗？
namespace mymuduo
{
namespace CurrentThread
{
    extern __thread int t_cachedTid;//保存tid（作为缓存），因为系统调用非常耗时，所以在拿到tid后将其保存下来

    void cacheTid();

    inline int tid() //内联函数只在当前文件中起作用
    {   //__builtin_expect是一种底层优化，此语句意思是如果还未获取tid，进入if后通过cacheTid()系统调用获取tid
        if(__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}

}