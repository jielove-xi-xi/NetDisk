#ifndef TIMER_H
#define TIMER_H
#include<unordered_map>
#include<vector>
#include <assert.h> 
#include<chrono>
#include<functional>
#include <sys/epoll.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
using namespace std;


//使用小根堆实现的定时器，主要作用于关闭长时间不操作客户端，减少服务器负载


typedef std::function<void()>TimeoutCallBack;       //超时事件回调函数
typedef std::chrono::high_resolution_clock Clock;   //高精度时钟对象
typedef std::chrono::milliseconds MS;               //毫秒级单位时间对象
typedef Clock::time_point TimeStamp;                //时间戳，用于返回当前时间


//定时器结点结构体，包含id,超时时间，超时回调函数
struct TimerNode{
    int id;         //定时器id
    TimeStamp expires;  //剩余时间数
    TimeoutCallBack cb; //回调函数

    //重载小于运算符，用于堆排序
    bool operator<(const TimerNode &t)
    {
        return expires<t.expires;
    }
};




/*
以最小堆作为存储结构的定时器类，可以设定一个超时时间，在超时后指向回调函数（如断开连接），内部保持有序。
按最近超时放在堆顶。内部使用映射加快查询速度。每个定时器拥有唯一id（如使用套接字fd作为定时器id）.
简单使用方法：先使用构造函数构造一个对象。再调用add(),添加,再调用GetNextTick返回是否有超时，有则调用tick()
把超时的删除。 或者直接调用doWork删除指定id定时器。
*/
class Timer
{
public:
    //构造函数，初始时候预留64个空间
    Timer(){heap_.reserve(64);}
    ~Timer(){clear();}              //析构函数，清空堆数据

    void adjust(int id, int newExpires);    //传递定时器id,并将其时间延迟到系统当前时间+timeout毫秒。    
    void add(int id, int timeOut, const TimeoutCallBack& cb); //指定一个定时器id,和初始超时时间，和回调函数，将其添加进堆中。
    void doWork(int id);        //传递定时器id，运行该定时器回调函数，并将其从堆中删除
    void clear();               //清空堆，将其重置
    void tick();                //循环清除全部超时结点 
    void pop();                 //删除堆的顶部结点
    int GetNextTick();          //返回最近超时的定时器剩余时间，如果以及有超时的返回0.堆为空返回-1
private:
    void del_(size_t i);        //传递指定节点的下标，并将其删除,删除后并调整堆结构
    void siftup_(size_t i);     //传递一个定时器在堆数组中的下标，并根据它的超时时间剩余数，往上移，保持最小堆结构
    bool siftdown_(size_t index, size_t n); //传递定时器在堆中的索引和堆长度，调整堆结构。如果该定时器超时时间大过子节点，则往下循环交换
    void SwapNode_(size_t i, size_t j);     //将堆中两个位置（数组中下标）的数据交互，并更新映射表的映射    
private:
    std::vector<TimerNode>heap_;    //构建堆的底层容器，存储定时器结点
    std::unordered_map<int,size_t>ref_; //定时器结点ID到堆位置的映射
};


#endif