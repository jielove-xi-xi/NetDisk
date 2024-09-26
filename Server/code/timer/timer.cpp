#include "timer.h"



// 传递定时器id,并将其时间延迟到系统当前时间+timeout毫秒。
void Timer::adjust(int id, int timeout)
{
    /* 调整指定id的结点 */
    assert(!heap_.empty() && ref_.count(id) > 0);   //如果堆为空，或者当前定时器在堆的位置小于0，返回错误
    heap_[ref_[id]].expires = Clock::now() + MS(timeout); // 将该定时器的超时剩余时间赋值为当前时间并加上timeout毫秒
    siftdown_(ref_[id], heap_.size());
}

//指定一个定时器id,和初始超时时间，和回调函数，将其添加进堆中。
void Timer::add(int id, int timeout, const TimeoutCallBack &cb)
{
    assert(id >= 0);
    size_t i;
    //映射表中，不存在该节点，则添加进去
    if(ref_.count(id) == 0) {       //count检查键是否存在，存在返回1，否则返回0
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i); //因为堆尾插入，需要向上调整
    } 
    else {
        /* 已有结点：调整堆，返回其中数组中索引，并延长超时时间，重置回调函数，然后调整堆结构 */
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        if(!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }    
}

//传递定时器id，运行该定时器回调函数，并将其从堆中删除
void Timer::doWork(int id)
{
    /* 删除指定id结点，并触发回调函数 */
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id]; //返回在堆中的下标
    TimerNode node = heap_[i];  //获取该节点
    node.cb();  //运行回调函数
    del_(i);    //删除该节点    
}

//清空堆，将其重置
void Timer::clear()
{
    ref_.clear();
    heap_.clear();    
}


/* 循环清除全部超时结点 */
void Timer::tick()
{
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        //将该结点的当前剩余时间-系统现在的时间，转换成毫秒，
        //在使用count获取数据差值。如果大于0，该结点没有超时，因为是最小堆，所以后面的也不会超时，直接结束循环
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break; 
        }
        node.cb();  //调用回调函数，进行超时处理
        pop();      //将该结点删除
    }    
}

//删除堆的顶部结点
void Timer::pop()
{
    assert(!heap_.empty());
    del_(0);    
}

//返回最近超时的定时器剩余时间，如果以及有超时的返回0.堆为空返回-1
int Timer::GetNextTick()
{
    tick(); //先处理已经超时的
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }    //如果返回0，说明有超时的，否则还没有超时，返回剩余时间。
    }
    return res;
}

//传递指定节点的下标，并将其删除,删除后并调整堆结构
void Timer::del_(size_t index)
{
    /* 删除指定位置的结点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;   //保持待删除节点下标
    size_t n = heap_.size() - 1;    //数据最大下标
    assert(i <= n);     
    if(i < n) {
        SwapNode_(i, n);        //将其交换到最后节点
        //交换之后需要重新调整处于i位置的节点，先向下调整，如果向下不需要调整，再向上。
        if(!siftdown_(i, n)) {  
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().id);   
    heap_.pop_back();    
}

//传递一个定时器在堆数组中的下标，并根据它的超时时间剩余数，往上移，保持最小堆结构
void Timer::siftup_(size_t i)
{
    assert(i >= 0 && i < heap_.size());
    while (i > 0) {
        size_t j = (i - 1) / 2; //定位父节点
        if (heap_[j] < heap_[i]) { break; }  //如果它比父节点大，则不需要他更新，直接返回
        SwapNode_(i, j);        //将其与父节点交换
        i = j;                  //更新下标指向父节点，继续往上调整
    }    
}



//传递定时器在堆中的索引和堆长度，调整堆结构。如果该定时器超时时间大过子节点，则往下循环交换，并返回是否有调整
bool Timer::siftdown_(size_t index, size_t n)
{
    assert(index >= 0 && index < heap_.size()); //确保下标和长度安全
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;           //保存索引
    size_t j = i * 2 + 1;       //获取其左子节点
    while(j < n) {              //往下循环调整堆结构，保存最小堆
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;       //确认左子节点更小还有右子节点更小，如果右子节点更小，指向右子节点
        if(heap_[i] < heap_[j]) break;  //如果子节点小于该节点，则不需要调整，直接返回
        SwapNode_(i, j);        //小于则交互，并更新i变量
        i = j;
        j = i * 2 + 1;          //更新j变量，继续往下调整，直到超出范围
    }
    return i > index;   //返回有没有进行调整，如果i==idnex，说明没有调整
}

//将堆中两个位置（数组中下标）的数据交互，并更新映射表的映射
void Timer::SwapNode_(size_t i, size_t j)
{
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]); //交互并更新映射表
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;    
}


