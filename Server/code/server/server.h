#ifndef SERVER_H
#define SERVER_H

#include"../Log/log.h"
#include"../pool/workQue.h"
#include"../sql/MyDb.h"
#include"../timer/timer.h"
#include"../tool/srtool.h"
#include"../tool/LongTaskTool.h"
#include"../tool/ShortTaskTool.h"
#include"../pool/ClientCon.h"
#include"../pool/UpDownCon.h"
#include"epoller.h"




//服务器类
class Server
{
public:
    Server(const char *host, int port,int upPort, const char *sqlUser, const char *sqlPwd, const char *dbName, 
int connPoolNum, int sqlport, int threadNum, int logqueSize,int _timeout,const char *EqualizerIP,int EqualizerPort,const char *EqualizerKey,const string servername,bool isConEqualizer);
    ~Server();
    void start();   

//主要功能函数
private:
    int tcpInit(const char *ip,int port,int updownPort);       
    void CloseCon(AbstractCon *client);                       
    void handleNewConnection(int select);                    
    void handleClientTask(AbstractCon *client);     
    void createFactory(AbstractCon *con);   
//辅助主要功能的函数
private:
    bool ConnectEqualizer(const string &EqualizerIP,const int &EqualizerPort,const string &MineIP,const int MiniSPort,const int &MiniLPort,const string &servername,const string &key);
    void sendServerState(int state);
    void TimerSendServerState(int millisecond);
    int  SetFdNonblock(int fd);
    void SendError(int fd,const char *info);
    void AddClient(int clientfd, SSL *ssl,int select);
    std::shared_ptr<AbstractTool>GetTool(PDU &pdu,AbstractCon *con);
    std::shared_ptr<AbstractTool>GetTool(TranPdu &pdu,AbstractCon *con);
//成员变量
private:
    std::unique_ptr<WorkQue> m_workque;      //线程池，工作队列，用于添加任务
    std::unique_ptr<Timer> m_timer;            //用于处理定时器超时，断开超时无操作连接
    std::unique_ptr<Epoller>m_epoll;            //epoll字柄

    std::unordered_map<int,std::shared_ptr<AbstractCon>>m_clientcon;    //套接字与用户连接对象的映射
    int sockfd;               //服务端监听sock,处理新连接，处理短任务。如登陆，注册
    int timeoutMS_;           //超时时间，单位毫秒
    static const int MAX_FD = 65536;    //最大文件描述符数
    
    SSL_CTX *m_ctx=nullptr;     //安全套接字
    uint32_t listenEvent_;      //监听套接字默认监控事件：EPOLLRDHUP（对端关闭） EPOLLIN(可读事件) EPOLLET（边缘触发）
    uint32_t connEvent_;        //客户端连接默认监控事件：//连接默认监控事件EPOLLONESHOT（避免多线程，下次需要从新设置监听） | EPOLLRDHUP

    //处理长耗时操作
    int udsockfd;             //处理处理上传和下载任务的sockfd描述符

    int SockEqualizer=0;      //服务器与负载均衡器交互sock
    bool EqualizerUsed=false;//负载均衡器是否使用
}; 

#endif