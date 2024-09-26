#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>


/*BlockDeque 是一个线程安全的阻塞队列模板类，使用生产者与消费者模型，适用于多线程环境下的数据传递与同步。
使用流程：先调用构造函数初始化队列最大空间，然后在生产者线程中调用push_back，把数据压入队列，然后唤醒消费者线程使用pop取出数据进行处理。需要往线程传递该对象引用
适合用于异步作业：如异步日记系统
*/
template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000); //模板构造函数，传递一个Size_t 构造阻塞队列最大空间
    ~BlockDeque();    //析构函数，将阻塞队列清空，并关闭双端队列，不在接受任何处理。并唤醒全部生产者和消费者线程

    bool empty();     //返回阻塞队列是否为空，空则为true
    bool full();      //返回阻塞队列是否满。满则为true
    size_t size();    //返回队列当前元素数量
    size_t capacity();  //返回队列总可用空间

    void Close();     //将阻塞队列清空，并关闭双端队列，不在接受任何处理。并唤醒全部生产者和消费者线程
    void clear();     //清空阻塞队列

    void push_back(const T &item);  //将元素item插入队列尾部，并唤醒消费者线程
    void push_front(const T &item); //将元素item插入到队列头部，并唤醒一个消费者线程
    T front();          //返回队头元素
    T back();           //返回队尾元素
    bool pop(T &item);              //将队列头部元素取出保存在item中，并返回是否成功
    bool pop(T &item, int timeout); //将队列头部元素取出保存在item中，并返回是否成功，非阻塞版本，等待超时则返回false
    void flush();                   //唤醒一个消费者进程，该线程对作业进行处理
private:
    std::deque<T> deq_;     //存储数据元素的双端队列
    size_t capacity_;       //队列的空间大小
    std::mutex mtx_;        //互斥量
    bool isClose_;          //是否关闭队列
    std::condition_variable condConsumer_;  //消费者等待条件变量
    std::condition_variable condProducer_;  //生产者等待条件变量
};


//模板构造函数，传递一个Size_t 构造阻塞队列最大空间
template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;       //默认为不关闭
}

//析构函数，将阻塞队列清空，并关闭双端队列，不在接受任何处理。并唤醒全部生产者和消费者线程
template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};


//将阻塞队列清空，并关闭双端队列，不在接受任何处理。并唤醒全部生产者和消费者线程
template<class T>
void BlockDeque<T>::Close() {
    {   
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
        isClose_ = true;
    }
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

//唤醒一个消费者进程，该线程对作业进行处理
template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

//清空阻塞队列
template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

//返回队头元素
template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

//返回队尾元素
template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

//返回队列当前元素数量
template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

//返回队列总可用空间
template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

//将元素item插入队列尾部，并唤醒消费者线程
template<class T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {   //如果队列已满
        condProducer_.wait(locker);     //生成者线程阻塞等待消费者线程释放空间
    }
    deq_.push_back(item);   
    condConsumer_.notify_one();     //唤醒一个消费者线程去使用
}

//将元素item插入到队列头部，并唤醒一个消费者线程
template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

//返回阻塞队列是否为空，空则为true
template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

//返回阻塞队列是否满。满则为true
template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

//将队列头部元素取出保存在item中，并返回是否成功
template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){        //如果队列为空，阻塞等待生产者线程唤醒
        condConsumer_.wait(locker); 
        if(isClose_){       //如果阻塞队列已经关闭，返回false
            return false;
        }
    }
    item = deq_.front();        //取出头部元素，并从队列中删除，然后唤醒一个生产者线程
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

//将队列头部元素取出保存在item中，并返回是否成功，非阻塞版本，等待超时则返回false
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){        //如果超时返回false
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H