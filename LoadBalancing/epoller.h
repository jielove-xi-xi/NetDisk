#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>


/*
Epoller类用于方便操作epoll事件的类。
使用方法：先构造-在AddFd将文件描述符和关联事件添加进epoll监控。再调用Wait等待事件就绪
*/
class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);  //构造函数，传递最大epoll最大事件数，构造epoll，默认为1024

    ~Epoller();

    bool AddFd(int fd, uint32_t events);    //传递文件描述符fd，和事件events,将该文件描述符和事件关联，添加到epoll监控

    bool ModFd(int fd, uint32_t events);    //将文件描述符fd,关联的事件修改

    bool DelFd(int fd);                     //从epoll中删除文件描述符fd

    int Wait(int timeoutMs = -1);           //传递超时时间，等待epoll事件返回就绪文件描述符就绪数量，超时返回。默认-1，阻塞等待

    int GetEventFd(size_t i) const;         //返回第i个epool事件关联的文件描述符

    uint32_t GetEvents(size_t i) const;     //返回第i个事件类型
        
private:
    int epollFd_;       //epoll 文件描述符

    std::vector<struct epoll_event> events_;    //存储epoll事件的vec
};

#endif //EPOLLER_H