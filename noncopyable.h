#pragma once //防止头文件重复包含

namespace mymuduo
{
/*
noncopyable被继承之后，派生类对象可以正常的构造和析构，但是派生类对象
    无法进行拷贝构造和拷贝赋值操作。
*/
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    void operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

}