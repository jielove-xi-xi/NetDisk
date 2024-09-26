#include "ClientCon.h"



//传递套接字和安全套接字构造
ClientCon::ClientCon(int sockFd,SSL *_ssl)
{
    m_clientsock=sockFd;
    m_clientSsl=_ssl;
    userCount++;
}

ClientCon::~ClientCon()
{
    Close();    //关闭连接
}

//初始化一个连接类
void ClientCon::init(const UserInfo &_info)
{
    m_userinfo=_info;
    m_isVip=(string(m_userinfo.isvip)=="1");
}



//关闭与客户端的连接，并释放ssl和sock
void ClientCon::Close()
{
    if(m_isClose==true)return;
    if (m_clientSsl != nullptr)
    {
        int shutdown_result = SSL_shutdown(m_clientSsl);
        if (shutdown_result == 0) // 等待客户端接收关闭ssl连接
        {
            shutdown_result = SSL_shutdown(m_clientSsl);
        }

        if (shutdown_result <0)
        {
            std::cerr << "SSL_shutdown failed with error code: " << SSL_get_error(m_clientSsl, shutdown_result) << std::endl;
        }
        SSL_free(m_clientSsl);
        m_clientSsl = nullptr; 
    }

    if (m_clientsock >= 0)
    {
        if (close(m_clientsock) < 0)
        {
            std::cerr << "close() failed with error: " << strerror(errno) << std::endl;
        }
    }
    userCount--;    //减去客户端连接数1
    m_isClose=true;
}


