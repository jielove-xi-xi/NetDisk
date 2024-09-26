#include"workQue.h"

WorkQue::WorkQue(size_t threadCount):isClose(false)
{
    assert(threadCount>0);
    for(size_t i=0;i<threadCount;++i)
    {
        std::thread([this]
        {
            std::unique_lock<std::mutex>locker(mutex);
            while(true)
            {
                if(!deq.empty())
                {
                    auto task=std::move(deq.front());
                    deq.pop();
                    locker.unlock();
                    if(task)
                        task();
                    locker.lock();
                }else if(isClose)break;
                else con.wait(locker);
            }
        }).detach();
    }
}

WorkQue::~WorkQue()
{
    while(true)
    {
        if(deq.size()==0){
            std::this_thread::sleep_for(std::chrono::seconds(3));
            break;
        }
    }
    std::lock_guard<std::mutex>locker(mutex);
    isClose=true;
    con.notify_all();
}




