#ifndef LONGTASKTOOL_H
#define LONGTASKTOOL_H

#include"AbstractTool.h"



//负责上传任务
class PutsTool:public AbstractTool{
public:
    PutsTool(const TranPdu &_pdu,AbstractCon *_con):m_pdu(_pdu),m_conParent(_con){}
    PutsTool(AbstractCon *_con):m_conParent(_con){}
    int doingTask();
private:
    bool FirstCheck(UpDownCon *m_con);  //首次连接认证
    UDtask CreateTask(ResponsePack &respon,MyDb &db);    //生成任务结构体
    void RecvFileData(UpDownCon *m_con);        //读取数据
private:
    TranPdu m_pdu;
    AbstractCon *m_conParent=nullptr;
};


class GetsTool:public AbstractTool
{
public:
    GetsTool(const TranPdu &_pdu,AbstractCon *_con):m_pdu(_pdu),m_con(dynamic_cast<UpDownCon*>(_con)){};
    GetsTool(AbstractCon *_con):m_con(dynamic_cast<UpDownCon*>(_con)){}
    int doingTask();
private:
    bool FirstCheck();      //首次连接认证
    bool CreateTask(ResponsePack &respon,UDtask &task, MyDb &db);
    void SendFileData();
private:
    TranPdu m_pdu;
    UpDownCon *m_con=nullptr;
};



#endif