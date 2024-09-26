#ifndef CLIENTCON_H
#define CLIENTCON_H


#include"AbstractCon.h"


class ClientCon:public AbstractCon{
public:
    ClientCon(int sockFd,SSL *_ssl);
    ClientCon()=default;
    ~ClientCon();

    void init(const UserInfo &info);
    void Close();
    
};






#endif