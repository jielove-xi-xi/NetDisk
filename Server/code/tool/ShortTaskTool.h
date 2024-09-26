#ifndef SPECIFICTOOL_H
#define SPECIFICTOOL_H
#include"AbstractTool.h"


//负责Login任务
class LoginTool:public AbstractTool{
public:
    LoginTool(const PDU &_pdu,AbstractCon *_con):m_pdu(_pdu),m_conParent(_con){}
    int doingTask();
private:
    PDU m_pdu;  //通信PDU
    AbstractCon *m_conParent=nullptr;
};


//负责Sign工具
class SignTool:public AbstractTool{
public:
    SignTool(const PDU &_pdu,AbstractCon *_con):m_pdu(_pdu),m_conParent(_con){}
    int doingTask();
private:
    bool createDir();
    PDU m_pdu;
    AbstractCon *m_conParent=nullptr;
};



//负责处理返回给客户端文件列表功能，仅限已经认证客户端操作
class CdTool:public AbstractTool{
public:
    CdTool(const PDU &_pdu,AbstractCon *_con):m_pdu(_pdu),m_conParent(_con){}
    int doingTask();
private:
    PDU m_pdu;
    AbstractCon *m_conParent=nullptr;
};

//负责创建文件夹
class CreateDirTool:public AbstractTool{
public:
    CreateDirTool(const PDU &_pdu,AbstractCon *_con):m_pdu(_pdu),m_con(static_cast<ClientCon*>(_con)){}
    int doingTask();
private:
    PDU m_pdu;
    ClientCon *m_con=nullptr;
};

//删除文件或者文件夹
class DeleteTool:public AbstractTool{
public:
    DeleteTool(const PDU &_pdu,AbstractCon *_con):m_pdu(_pdu),m_con(static_cast<ClientCon*>(_con)){}
    int doingTask();
private:
    PDU m_pdu;
    ClientCon *m_con=nullptr;
};

#endif