#ifndef ABSTRACTCON_H
#define ABSTRACTCON_H

#include"../function.h"
#include<atomic>
#include<assert.h>
#include"../Log/log.h"

class AbstractCon{
public:
    static std::atomic<int>userCount;       //原子类型的连接总数，用来保存整个服务器总连接数
    AbstractCon()=default;
    virtual ~AbstractCon(){}                //注意：这里必须是虚析构函数，否则会内存泄漏
    virtual void Close()=0;                 

    void setVerify(const bool &status);
    SSL *getSSL()const;
    int getsock()const;
    string getuser()const;
    string getpwd()const;
    bool getisVip()const;
    bool getIsVerify()const;
    void closessl();

    enum ConType{SHOTTASK=0,LONGTASK,PUTTASK,GETTASK,GETTASKWAITCHECK};
    int ClientType=0;       //客户端类型，0为短任务，1长任务，2为上传，3为下载,4为下载完毕等待客户端检查文件数据无误
protected:
    SSL *m_clientSsl=nullptr;     //客户端ssl套接字
    int m_clientsock;             //客户端sock
    UserInfo m_userinfo;          //用户信息体
    bool m_isVerify=false;        //客户端是否已经进行登陆认证，用来确定能否进行其它操作
    bool m_isClose=false;         //客户端是否已经关闭
    bool m_isVip=false;           //是否vip用户
};













#endif