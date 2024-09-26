#include "AbstractCon.h"

std::atomic<int>AbstractCon::userCount(0); //初始化全局变量

void AbstractCon::setVerify(const bool & status)
{
    m_isVerify=status;
}

SSL *AbstractCon::getSSL() const
{
    return m_clientSsl;
}

int AbstractCon::getsock() const
{
    return m_clientsock;
}

string AbstractCon::getuser() const
{
    return string(m_userinfo.username);
}

string AbstractCon::getpwd() const
{
    return string(m_userinfo.pwd);
}

bool AbstractCon::getisVip() const
{
    return m_isVip;
}

bool AbstractCon::getIsVerify() const
{
    return m_isVerify;
}

//发送关闭ssl安全套接字请求
void AbstractCon::closessl()
{
    if(m_clientSsl!=nullptr)
    {
        int shutdown_code=SSL_shutdown(m_clientSsl);
        if(shutdown_code<0)
        {
            std::cout<<"向客户端:"<<m_userinfo.username<<" 发送关闭ssl连接请求出错"<<endl;
        }
    }
}
