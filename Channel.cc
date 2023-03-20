#include <sys/epoll.h>
#include <cassert>

#include "Channel.h"
#include "Logger.h"
#include "EventLoop.h"

using namespace mymuduo;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop : ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    :loop_(loop),
    fd_(fd),
    events_(0),
    revents_(0),
    index_(-1),
    tied_(false),
    eventHandling_(false),
    //这个数据成员不在这个时候初始化吗？好像是update()的时候更新
    addedToLoop_(false)
{
}

Channel::~Channel()
{
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if(loop_->isInLoopThread())
    {
        assert(!loop_->hasChannel(this));
    }
}

// 注意什么时候调用(新连接创建的时候) TcpConnection=>Channel
/*
    channel的回调函数从TcpConnection中来
    TcpConnection中注册了Channel对应的回调函数，传入的回调函数均为TcpConnection
    对象的成员方法，因此可以说明的是：Channel的结束一定早于TcpConnection对象！
    此处用tie()解决TcpConnection和Channel的生命周期时长问题，从而保证了Channel对
    象能够在TcpConnection销毁前销毁
*/
void Channel::tie(const std::shared_ptr<void> &obj)
{
    //tie_拥有TcpConnection的弱指针引用
    tie_ = obj;
    tied_ = true;
}

//当改变Channel所管理的fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
void Channel::update()
{
    addedToLoop_ = true;
    //通过channel所属的eventloop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在channel所属的loop中，将当前的channel删除掉
void Channel::remove()
{
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void> guard;
        guard = tie_.lock();
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
        //如果提升失败，说明Channel关联的TcpConnection对象已经不在了，就不做任何处理
    }
    //这里是干嘛的？为什么没有tie还有执行？
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

// 根据poller通知的channel发生的具体事件，执行回调
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d", revents_);
    eventHandling_ = true;
    //关闭
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))//当TcpConnection对应Channel通过shutdown关闭写端，epoll出发EPOLLHUP
    {
        if(closeCallback_)  closeCallback_();
        
    }
    //错误
    if(revents_ & EPOLLERR)
    {
        if(errorCallback_)  errorCallback_();

    }
    //读
    if(revents_ & (EPOLLIN | EPOLLPRI))
    {
        if(readCallback_)   readCallback_(receiveTime);
    }
    //写
    if(revents_ & EPOLLOUT)
    {
        if(writeCallback_)  writeCallback_();
    }
    eventHandling_ = false;
}