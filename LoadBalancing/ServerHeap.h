#ifndef SERVERHEAP_H
#define SERVERHEAP_H
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


struct ServerNode{
    std::string servername; //服务器名
    size_t curConCount=0;   //当前连接数
    std::string ip;         //ip地址
    uint16_t shotPort=0;    //短任务端口
    uint16_t longPort=0;    //长任务端口
    int sock=-1;            //与均衡器交互的sock对象
    bool isClose=false;     //是否已经关闭

    ServerNode(const size_t &count,const std::string &addr,const uint16_t &sPort,const uint16_t &lPort,const int &sock_,const string &_servername)
        :curConCount(count),ip(addr),shotPort(sPort),longPort(lPort),sock(sock_),servername(_servername){}
    ServerNode()=default;
    //重载小于运算符，用于堆排序
    bool operator<(const ServerNode&other)const{
        return curConCount<other.curConCount;
    }
};

class ServerHeap
{
public:
    //构造函数，初始时候预留64个空间
    ServerHeap(){heap_.reserve(64);}
    ~ServerHeap(){clear();}              

    void adjust(int ServerSock, size_t curSerConCount);    
    void add(int ServerSock, const std::string &addr,const uint16_t &sPort,const uint16_t &lPort,const size_t curConCount,const string &servername);             
    void GetMinServerInfo(ServerNode &info);  
    void pop(int sock);   
    void clear();      
    std::vector<ServerNode>GetVet();            
private:
    void del_(size_t i);                    
    void siftup_(size_t i);                 
    bool siftdown_(size_t index, size_t n); 
    void SwapNode_(size_t i, size_t j);      
private:
    std::vector<ServerNode>heap_;           //构建堆的底层容器，存储服务器节点
    std::unordered_map<int,size_t>ref_;     //服务器结点ID到堆位置的映射，id就是服务器对应的sock
};

#endif