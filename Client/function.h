#pragma once
/*
* 公共头文件，用来保存客户端与服务端通信协议等相同代码
*/
#include <iostream>
#include <QWidget>
#include<QListWidget>
#include<QDataStream>
#include<QVariant>

#define MainServerIP "127.0.0.1"    //负载均衡器地址
#define MainServerPort 9091         //负载均衡器端口
#define DefaultserverIP "127.0.0.1" //默认服务器地址，负载均衡器无法工作，将会默认使用它
#define DefaultserverSPort 8080
#define DefaultserverLPort 8081


#define USERSCOLLEN 8            //数据库用户信息表列数
#define USERSCOLMAXSIZE 50      //数据库用户信息表每列的最大长度

//状态码
enum Code {
    LogIn = 1,    //客户端要进行登陆操作
    SignIn,     //客户端要进行注册操作
    Puts,       //客户端要进行上传操作
    Gets,       //客户端要进行下载操作
    CD,         //客户端要读取文件信息
    MAKEDIR,    //客户端要创建文件夹
    DELETEFILE,     //客户端要删除文件

    CodeOK,    //回复OK
    CodeNO,    //回复NO
    ClientShut, //客户端关闭
    CapacityNO, //云盘空间不足
    FileNOExist,//文件不存在

    PutsContinue,   //断点上传
    PutsContinueNO,   //断点上传失败，从0开始
    PutQUICKOK,       //相同的文件已经存在，妙传
    GetsContinueNO,     //断点下载失败
};

// 与服务端进行短任务交互的协议单元：如进行登陆、注册、删除、创建文件夹等功能
struct PDU
{   
    int PDUcode = 0;            //状态码
    char user[20] = { 0 };      //用户名，默认最长20，非中文
    char pwd[20] = { 0 };       //密码，默认最长20，非中文
    char filename[100] = { 0 }; //可用,可不用。如删除文件，就使用。最长100字节
    int len = 0;                //备用空间长度，如有特别要求则使用，最长100字节
    char reserve[200] = { 0 };  //备用空间
};
#define PDULEN  348              //定义pdu最大长度为348
#define PDUReserlen  200            //PDU预留空间长度


// 服务器简版回复客户端包体，不用每次携带大量数据
struct ResponsePack
{
    int code = 0;           //状态码
    int len = 0;            //备用空间存有数据量长度
    char buf[200] = { 0 };  //备用空间，最大200字节
};
#define Responlen  208     //回复体最大长度
#define ResBuflen  200     //回复体备用空间长度

//客户端信息结构体，用来保存从服务器接收的用户信息
struct UserInfo
{
    char username[USERSCOLMAXSIZE] = { 0 };     //用户名
    char pwd[USERSCOLMAXSIZE] = { 0 };          //密码
    char cipher[USERSCOLMAXSIZE] = { 0 };       //哈希密码，用来提供简易认证
    char isvip[USERSCOLMAXSIZE] = { 0 };        //是否VIP
    char capacitySum[USERSCOLMAXSIZE] = { 0 };  //云盘总空间大小
    char usedCapacity[USERSCOLMAXSIZE] = { 0 }; //云盘已用空间大小
    char salt[USERSCOLMAXSIZE] = { 0 };         //可用使用来提供多重认证
    char vipdate[USERSCOLMAXSIZE] = { 0 };      //会员到期时间
};
#define UserInfoLen USERSCOLLEN*USERSCOLMAXSIZE


//文件信息体,即保存在数据库中的文件和文件夹
struct FILEINFO {
    std::uint64_t FileId=0;       //文件在数据库的id
    char Filename[100] = { 0 };     //文件名，最长100字节
    int DirGrade = 0;             //目录等级，距离根目录距离，用来构建文件给客户端界面系统
    char FileType[10] = { 0 };      //文件类型，如d为目录，f为文件，也可以拓展MP3等
    std::uint64_t FileSize=0;     //文件大小
    std::uint64_t ParentDir=0;    //父文件夹的ID，指明文件保存在哪个文件夹下面
    char FileDate[100] = { 0 };   //文件的上传日期
    //默认构造函数
    FILEINFO() = default;
    // 带参数构造函数（用于方便初始化）
    FILEINFO(std::uint64_t id, const char* name, int grade, const char* type, std::uint64_t size, std::uint64_t parent,const char *date)
        : FileId(id), DirGrade(grade), FileSize(size), ParentDir(parent) {
        strncpy(Filename, name, sizeof(Filename) - 1);
        strncpy(FileType, type, sizeof(FileType) - 1);
        strncpy(FileDate, date, sizeof(FileDate) - 1);
    }
};
#define FILEINFOLEN 238

//用于文件上传和下载的通信协议
struct TranPdu {
    int TranPduCode = 0;  //命令
    char user[20] = { 0 };  //用户名
    char pwd[20] = { 0 };   //密码
    char filename[100] = { 0 }; //文件名
    char FileMd5[100] = { 0 };  //文件MD5码
    std::uint64_t filesize = 0;   //文件长度
    std::uint64_t sendedsize = 0; //实现断点续传的长度
    std::uint64_t parentDirID = 0;    //保存在哪个目录下的ID，为0则保存在根目录下
    static inline bool TranPduToArr(TranPdu& pdu, char* buf);  //通信协议转换成buf数组
};
#define TranPduLen 268

inline bool TranPdu::TranPduToArr(TranPdu& pdu, char* buf)
{
    // 检查缓冲区是否为 nullptr 或者大小是否足够
    int offset = 0;
    memcpy(buf + offset, &pdu.TranPduCode, sizeof(pdu.TranPduCode));
    offset += sizeof(pdu.TranPduCode);
    memcpy(buf + offset, pdu.user, sizeof(pdu.user));
    offset += sizeof(pdu.user);
    memcpy(buf + offset, pdu.pwd, sizeof(pdu.pwd));
    offset += sizeof(pdu.pwd);
    memcpy(buf + offset, pdu.filename, sizeof(pdu.filename));
    offset += sizeof(pdu.filename);
    memcpy(buf + offset, pdu.FileMd5, sizeof(pdu.FileMd5));
    offset += sizeof(pdu.FileMd5);
    memcpy(buf + offset, &pdu.filesize, sizeof(pdu.filesize));
    offset += sizeof(pdu.filesize);
    memcpy(buf + offset, &pdu.sendedsize, sizeof(pdu.sendedsize));
    offset += sizeof(pdu.sendedsize);
    memcpy(buf + offset, &pdu.parentDirID, sizeof(pdu.parentDirID));
    offset += sizeof(pdu.parentDirID);
    return true;
}


//将pdu转换为一个字节数组
inline bool PduToArr(const PDU& pdu, char* array)
{
    memset(array, 0,PDULEN);
    size_t offset = 0;
    memcpy(array + offset, &pdu.PDUcode, sizeof(pdu.PDUcode));
    offset += sizeof(pdu.PDUcode);
    memcpy(array + offset, pdu.user, sizeof(pdu.user));
    offset += sizeof(pdu.user);
    memcpy(array + offset, pdu.pwd, sizeof(pdu.pwd));
    offset += sizeof(pdu.pwd);
    memcpy(array + offset, pdu.filename, sizeof(pdu.filename));
    offset += sizeof(pdu.filename);
    memcpy(array + offset, &pdu.len, sizeof(pdu.len));
    offset += sizeof(pdu.len);

    if(pdu.len>0)
        memcpy(array + offset, pdu.reserve, pdu.len);
    return true;
}

//将一个char指针转为ResponsePack
inline bool ArrToRespon(char* buf,ResponsePack &res)
{
    size_t offset = 0;
    memcpy(&res.code, buf+offset, sizeof(res.code));
    offset += sizeof(res.code);
    memcpy(&res.len, buf +offset, sizeof(res.len));
    offset += sizeof(res.code);
    if (res.len > 0)
        memcpy(res.buf, buf + offset, res.len);
    return true;
}


inline QString formatFileSize(quint64 size)
{
    const quint64 KB = 1024;
    const quint64 MB = KB * 1024;
    const quint64 GB = MB * 1024;

    if (size >= GB) {
        return QString::asprintf("%.2f G", static_cast<double>(size) / GB);
    }
    else if (size >= MB) {
        return QString::asprintf("%.2f M", static_cast<double>(size) / MB);
    }
    else if (size >= KB) {
        return QString::asprintf("%.2f K", static_cast<double>(size) / KB);
    }
    else {
        return QString::asprintf("%llu B", size);
    }
}


struct ItemDate {
    quint64 ID = 0;
    QString filename;
    QString filetype;
    quint64 Grade = 0;      //文件距离根文件夹的距离，用来排序。构建文件系统
    quint64 parentID = 0;
    quint64 filesize = 0;
    QString filedate;

    QListWidget* parent = nullptr;      //父文件夹（前驱文件夹）指针
    QListWidget* child = nullptr;       //本文件夹指针，即保存该文件夹中的项的指针
    ItemDate() = default;
};

