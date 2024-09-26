#ifndef WORKQUE_H
#define WORKQUE_H

#include<mutex>
#include<condition_variable>
#include<assert.h>
#include<thread>
#include<functional>
#include"../tool/LongTaskTool.h"
#include"../tool/ShortTaskTool.h"

//工作队列，线程池

//任务队列
class WorkQue{
public:
    explicit WorkQue(size_t threadCount=10);
    WorkQue()=default;
    WorkQue(WorkQue &&)=default;
    ~WorkQue();

    template<class F>                        //往线程池添加作业
    void AddTask(F &&task);
private:
    std::mutex mutex;   //互斥量
    std::condition_variable con;    //条件变量
    std::queue<std::function<void()>>deq;                 //任务队列
    bool isClose=false;                   //线程池是否关闭，1为关闭
};

//往线程池添加作业
template <class F>
inline void WorkQue::AddTask(F &&task)
{
    if(isClose==true)return;
    {
        std::lock_guard<std::mutex>locker(mutex);
        deq.emplace(std::forward<F>(task));
    }
    con.notify_one();
}

#endif