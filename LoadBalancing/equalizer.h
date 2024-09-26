#ifndef EQUALIZER_H
#define EQUALIZER_H
#include<unordered_map>
#include<queue>
#include <stddef.h>
#include <string>
#include<assert.h>
#include<memory>
#include <netinet/in.h>
#include <bits/socket.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include"epoller.h"
#include"ServerHeap.h"

struct ServerInfoPack{
    char servername[30]={0};
    char ip[20]={0};
    int sport;
    int lport;
    std::uint64_t curConCount=0;
    ServerInfoPack(const string &_ip,const int &_sport,const int &_lport,const string &_servername):sport(_sport),lport(_lport)
    {
        size_t len=_ip.size()>20?20:_ip.size();
        memcpy(ip,_ip.c_str(),len);

        len=_servername.size()>30?30:_servername.size();
        memcpy(servername,_servername.c_str(),len);
    }
    ServerInfoPack()=default;
};


#pragma pack(push, 1)
struct ServerState {
    int code = 0;                     // 状态码，0为正常，1为关闭
    std::uint64_t curConCount = 0;    // 当前连接数
};
#pragma pack(pop)   //禁止内存对齐，避免发送数据出错

class Equalizer{
public:
    Equalizer(std::string ip,int Port_server,int Port_Client,string key);
    ~Equalizer();
    void start();
private:
    bool tcpInit(std::string &ip,const int &port_server,const int &port_client);
    void handleNewServerConnection();         //处理新服务器连接
    void handleNewClientConnection();         //处理新客户端请求  

    void recvServerInfo(int sock);
    void recvServerPack(int sock);
    void updateServerState(int sock,size_t newConCount);
    void addServer(int sock,const size_t &count,const std::string &addr,const uint16_t &sPort,const uint16_t &lPort,const string &servername);
    void delServer(int sock);
    void sendServerInfoToClient(int sock);

    int SetFdNonblock(int sock);
    void displayServerInfo();
private:
    int m_sockAboutServer;        //与服务器通信的sock
    int m_sockAboutClient;        //与客户端通信的sock
    Epoller m_epoll;              //epll对象
    ServerHeap m_serverHeap;      //保存服务器数据的堆

    uint32_t listenEvent_=EPOLLRDHUP|EPOLLHUP|EPOLLERR|EPOLLIN|EPOLLET;        //监听事件类型

    string ConKey;      //连接负载均衡器的密码
};

#endif
