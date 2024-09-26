#pragma once

#include <QObject>
#include<mutex>
#include<queue>
#include"../function.h"

//短任务队列
class TaskQue {
public:
	explicit TaskQue(size_t threadCount = 1);
	TaskQue() = default;
	TaskQue(TaskQue&&) = default;
	~TaskQue();

	void Close();	//关闭队列
	template<class F>
	void AddTask(F&& task);
private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::queue<std::function<void()>>m_deq;		//任务队列，存储函数对象
	bool m_isClose = false;						//任务队列是否关闭

	std::vector<std::thread>m_workers;		//保存工作线程的容器
	void Worker();	//工作线程处理函数
};


//往任务池添加任务
template<class F>
inline void TaskQue::AddTask(F&& task)
{
	if (m_isClose == true)return;
	{
		std::lock_guard<std::mutex>locker(m_mutex);
		m_deq.emplace(std::forward<F>(task));
	}
	m_cv.notify_one();
}
