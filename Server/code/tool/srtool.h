#ifndef SRTOOL_H
#define SRTOOL_H
#include"../function.h"




//send与recv类，主要用于网络发送和接受数据
class SR_Tool
{
public:
    SR_Tool()=default;
    ~SR_Tool()=default;

    size_t sendPDU(SSL *ssl,const PDU &pdu); //安全套接字层发送PDU
    size_t recvPDU(SSL *ssl,PDU &pdu);                  //安全套接字接收PDU
    
    size_t recvTranPdu(SSL *ssl,TranPdu &pdu);          //安全套接字接受TranPdu

    size_t sendReponse(SSL *ssl,const ResponsePack &response);  //安全套接字服务器回复简易包体
    size_t recvReponse(SSL *ssl,ResponsePack &response);  //安全接收客户端简易响应包体

    size_t sendUserInfo(SSL *ss,const UserInfo &info);  //使用ssl发生客户信息

    bool sendFileInfo(SSL *ssl,const vector<FILEINFO>&vet); //使用ssl把文件信息全部发送回客户端
};





#endif