#include "TaskQue.h"

TaskQue::TaskQue(size_t threadCount)
{
    for (size_t i = 0; i < threadCount; ++i)
        m_workers.emplace_back(&TaskQue::Worker, this); //创建i条线程
}

TaskQue::~TaskQue()
{
    Close();
    for (std::thread& worker : m_workers)
    {
        if (worker.joinable())
            worker.join();      //回收线程资源
    }
}

//关闭线程池
void TaskQue::Close()
{
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_isClose = true;
    }
    m_cv.notify_all();  // 通知所有线程
}

//工作函数
void TaskQue::Worker() 
{
    while (true) 
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> locker(m_mutex);
            m_cv.wait(locker, [this]() { return m_isClose || !m_deq.empty(); });

            if (m_isClose && m_deq.empty()) return;  // 队列关闭且无任务时退出

            task = std::move(m_deq.front());
            m_deq.pop();
        }
        if (task) 
            task();  // 执行任务
    }
}
