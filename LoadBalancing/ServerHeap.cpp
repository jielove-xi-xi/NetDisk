#include"ServerHeap.h"

//传递与服务器关联的sock，将其当前连接数修改
void ServerHeap::adjust(int serverSock, size_t curSerConCount)
{
    assert(!heap_.empty() && ref_.count(serverSock) > 0);   
    heap_[ref_[serverSock]].curConCount = curSerConCount;   
    if(!siftdown_(ref_[serverSock], heap_.size())) {
        siftup_(ref_[serverSock]);  // 如果无法下移，则可能需要上移
    }
}


//传递一个服务器sock,和地址与两个端口，将其添加进堆中
void ServerHeap::add(int ServerSock, const std::string &addr,const uint16_t &sPort,const uint16_t &lPort,const size_t curConCount,const string &servername)
{
    assert(ServerSock >= 0);
    size_t i;
    //映射表中，不存在该节点，则添加进去
    if(ref_.count(ServerSock) == 0) {       //count检查键是否存在，存在返回1，否则返回0
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[ServerSock] = i;
        heap_.push_back({curConCount,addr,sPort,lPort,ServerSock,servername});
        siftup_(i); 
    } 
    else {
        i = ref_[ServerSock];
        heap_[i].curConCount=curConCount;
        heap_[i].ip=addr;
        heap_[i].shotPort=sPort;
        heap_[i].longPort=lPort;
        heap_[i].sock=ServerSock;
        heap_[i].servername=servername;
        if(!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }    
}

//获取连接数最小服务器信息
void ServerHeap::GetMinServerInfo(ServerNode &info)
{
    if(heap_.size()>0)
        info=heap_[0];      //返回堆顶元素
}

//删除指定节点
void ServerHeap::pop(int sock)
{
    if(ref_.count(sock) == 0) return;  // 确保 sock 存在于映射中
    int index = ref_[sock];
    del_(index);
}

//清空堆
void ServerHeap::clear()
{
    ref_.clear();
    heap_.clear();
}

std::vector<ServerNode> ServerHeap::GetVet()
{
    return heap_;
}

//传递指定节点的下标（即sock)，并将其删除,删除后并调整堆结构
void ServerHeap::del_(size_t index)
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
    ref_.erase(heap_.back().sock);   
    heap_.pop_back();
}

//传递一个节点在堆数组中的下标，并根据它的当前连接数，往上移，保持最小堆结构
void ServerHeap::siftup_(size_t i)
{
    assert(i >= 0 && i < heap_.size());
    while(i > 0) {                 // 循环直到根节点，i == 0 时停止
        size_t j = (i - 1) / 2;    // 定位父节点
        if(heap_[j] < heap_[i]) { break; }  // 父节点已小于当前节点，结束调整
        SwapNode_(i, j);           // 交换
        i = j;                     // 更新当前节点索引
    }
}

//传递节点在堆中的索引和堆长度，调整堆结构。如果该节点当前连接数大过子节点，则往下循环交换
bool ServerHeap::siftdown_(size_t index, size_t n)
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
void ServerHeap::SwapNode_(size_t i, size_t j)
{
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]); //交互并更新映射表
    ref_[heap_[i].sock] = i;
    ref_[heap_[j].sock] = j;    
}