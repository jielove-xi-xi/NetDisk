#include "srtool.h"


//安全套接字层发送PDU,返回发送的字节数
size_t SR_Tool::sendPDU(SSL *ssl, const PDU &pdu)
{
    size_t ret=SSL_write(ssl,&pdu.PDUcode,sizeof(int));    //依次发送code 直到 reserver
    ret+=SSL_write(ssl,pdu.user,sizeof(pdu.user));
    ret+=SSL_write(ssl,pdu.pwd,sizeof(pdu.pwd));
    ret+=SSL_write(ssl,pdu.filename,sizeof(pdu.filename));
    ret+=SSL_write(ssl,&pdu.len,sizeof(int));

    if(pdu.len>0)   //如果使用了预留空间
        ret+=SSL_write(ssl,pdu.reserve,pdu.len);
    return   ret;
}


//安全套接字接收PDU，返回读取的长度
size_t SR_Tool::recvPDU(SSL *ssl, PDU &pdu)
{
    size_t ret=SSL_read(ssl,&pdu.PDUcode,sizeof(int));    //依次发送code 直到 reserver
    ret+=SSL_read(ssl,pdu.user,sizeof(pdu.user));
    ret+=SSL_read(ssl,pdu.pwd,sizeof(pdu.pwd));
    ret+=SSL_read(ssl,pdu.filename,sizeof(pdu.filename));
    ret+=SSL_read(ssl,&pdu.len,sizeof(int));

    if(pdu.len>0)   //如果使用了预留空间
        ret+=SSL_read(ssl,pdu.reserve,pdu.len);
    return   ret;
}


//安全套接字接受传输PDU
size_t SR_Tool::recvTranPdu(SSL *ssl, TranPdu &pdu)
{
    size_t ret=SSL_read(ssl,&pdu.TranPduCode,sizeof(int));
    ret+=SSL_read(ssl,pdu.user,sizeof(pdu.user));
    ret+=SSL_read(ssl,pdu.pwd,sizeof(pdu.pwd));
    ret+=SSL_read(ssl,pdu.filename,sizeof(pdu.filename));
    ret+=SSL_read(ssl,pdu.FileMd5,sizeof(pdu.FileMd5));
    ret+=SSL_read(ssl,&pdu.filesize,sizeof(pdu.sendedsize));
    ret+=SSL_read(ssl,&pdu.sendedsize,sizeof(pdu.sendedsize));
    ret+=SSL_read(ssl,&pdu.parentDirID,sizeof(pdu.parentDirID));

    return ret;
}

//安全套接字发送简易版回复体，返回发送字节数
size_t SR_Tool::sendReponse(SSL *ssl, const ResponsePack &response)
{
    SSL_write(ssl,&response.code,sizeof(response.code));
    SSL_write(ssl,&response.len,sizeof(response.code));
    if(response.len>0)
        SSL_write(ssl,response.buf,response.len);
    return response.len+8;
}


//安全接收客户端简易响应
size_t SR_Tool::recvReponse(SSL *ssl, ResponsePack &response)
{
    SSL_read(ssl,&response.code,sizeof(response.code));
    SSL_read(ssl,&response.len,sizeof(response.code));
    if(response.len>0)
        SSL_read(ssl,response.buf,response.len);
    return response.len+9;
}

//ssl发生客户端信息
size_t SR_Tool::sendUserInfo(SSL *ssl, const UserInfo &info)
{

    size_t ret=SSL_write(ssl,info.username,USERSCOLMAXSIZE);    
    ret+=SSL_write(ssl,info.pwd,USERSCOLMAXSIZE);  
    ret+=SSL_write(ssl,info.cipher,USERSCOLMAXSIZE);  
    ret+=SSL_write(ssl,info.isvip,USERSCOLMAXSIZE);  
    ret+=SSL_write(ssl,info.capacitySum,USERSCOLMAXSIZE);  
    ret+=SSL_write(ssl,info.usedCapacity,USERSCOLMAXSIZE);  
    ret+=SSL_write(ssl,info.salt,USERSCOLMAXSIZE);  
    ret+=SSL_write(ssl,info.vipdate,USERSCOLMAXSIZE); 
    return USERSCOLMAXSIZE*USERSCOLLEN; 
}

//将一个存在全部文件信息的向量发送回客户端
bool SR_Tool::sendFileInfo(SSL *ssl, const std::vector<FILEINFO> &vet) {
    // 遍历文件信息结构体的向量
    for (const auto &fileInfo : vet) {
        // 发送 FileId
        if (SSL_write(ssl, &fileInfo.FileId, sizeof(fileInfo.FileId)) <= 0) {
            return false;  // 如果发送失败，返回 false
        }

        // 发送 Filename (需要确保是固定长度 100 字节)
        if (SSL_write(ssl, fileInfo.Filename, sizeof(fileInfo.Filename)) <= 0) {
            return false;  // 如果发送失败，返回 false
        }

        // 发送 DirGrade
        if (SSL_write(ssl, &fileInfo.DirGrade, sizeof(fileInfo.DirGrade)) <= 0) {
            return false;  // 如果发送失败，返回 false
        }

        // 发送 FileType (需要确保是固定长度 10 字节)
        if (SSL_write(ssl, fileInfo.FileType, sizeof(fileInfo.FileType)) <= 0) {
            return false;  // 如果发送失败，返回 false
        }

        // 发送 FileSize
        if (SSL_write(ssl, &fileInfo.FileSize, sizeof(fileInfo.FileSize)) <= 0) {
            return false;  // 如果发送失败，返回 false
        }

        // 发送 ParentDir
        if (SSL_write(ssl, &fileInfo.ParentDir, sizeof(fileInfo.ParentDir)) <= 0) {
            return false;  // 如果发送失败，返回 false
        }

        if (SSL_write(ssl, &fileInfo.FileDate, sizeof(fileInfo.FileDate)) <= 0) {
            return false;  // 如果发送失败，返回 false
        }
        
    }
    
    return true;  // 如果所有数据发送成功，返回 true
}
