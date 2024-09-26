#ifndef FUNCTION_H
#define FUNCTION_H

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <deque>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include<unordered_map>
#include<openssl/ssl.h>
#include<openssl/err.h>
using namespace std;

/*
定义程序的全局公有结构体和函数以及包含的头文件

*/

//用于检查函数参数和返回值是否正确的宏
//检查传入的参数是否正确，如果不正确，则打印错误信息并返回1
#define ARGS_CHECK(argc,val)    \
{                               \
    if(argc!=val)               \
    {                           \
        printf("Args ERROR\n"); \
        return -1;              \
    }                           \
}

//此宏用于检查线程函数的返回值。如果不为0，则打印错误信息，并返回-1
#define THREAD_ERROR_CHECK(ret,funcName)            \
{                                                   \
    if(ret!=0)                                      \
    {                                               \
        printf("%s:%s\n",funcName,strerror(ret));   \
        return -1;                                  \
    }                                               \
}

//该宏用来检查函数返回值ret是否等于retval，如果等于输出错误信息filename,并返回-1
#define ERROR_CHECK(ret, retval, filename) \
{                                          \
        if (ret == retval)                 \
        {                                  \
            perror(filename);              \
            return -1;                     \
        }                                  \
}

/*=================================================通信结构体和通信函数===========================================*/

#define IP "127.0.0.1"
#define PORT 8080
#define THREADNUM   10  //线程数
#define MAXEVENTS   10  //events事件数量
#define USERSCOLLEN 8            //数据库用户信息表列数
#define USERSCOLMAXSIZE 50      //数据库用户信息表每列的最大长度
#define LOGPATH "./logfile"     //日记保存路径
#define ROOTFILEPATH "rootfiles"  //保存用户根文件路径

//服务端主进程和客户端通信协议
struct PDU
{
    int PDUcode=0;            //命令，如login、sign等
    char user[20]={0};      //用户名，非中文，最大20字节
    char pwd[20]={0};       //密码，初始未明文密码，登陆后为hash认证码
    char filename[100]={0};     //文件名（未使用MD5算法加密的），最大长度100，可中文
    int len=0;              //备用空间长度，如果大于0，说明reserve有数据需要读取
    char reserve[200]={0}; //备用空间缓存区
};


//用于文件上传和下载的通信协议
struct TranPdu{
    int TranPduCode=0;  //命令
    char user[20]={0};  //用户名
    char pwd[20]={0};   //密码
    char filename[100]={0}; //文件名
    char FileMd5[100]={0};  //文件MD5码
    std::uint64_t filesize=0;   //文件长度
    std::uint64_t sendedsize=0; //实现断点续传的长度
    std::uint64_t parentDirID=0;    //保存在哪个目录下的ID，为0则保存在根目录下.也可在进行下载任务时候指定要下载的文件ID
};

//状态码
enum Code{
    LogIn=1,    //客户端要进行登陆操作
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


//服务器简版回复客户端包体
struct ResponsePack
{
    int code=0;         //状态码
    int len=0;          //缓存空间大小  
    char buf[200]={0};  //最长200
};


//用户信息结构体，对应数据库用户表
struct UserInfo
{
    char username[USERSCOLMAXSIZE]={0};
    char pwd[USERSCOLMAXSIZE]={0};
    char cipher[USERSCOLMAXSIZE]={0};
    char isvip[USERSCOLMAXSIZE]={0};
    char capacitySum[USERSCOLMAXSIZE]={0};
    char usedCapacity[USERSCOLMAXSIZE]={0};
    char salt[USERSCOLMAXSIZE]={0};
    char vipdate[USERSCOLMAXSIZE]={0};  
};

//将数据库检查回来的信息转换为用户信息结构
inline bool ConvertTouserInfo(vector<std::string>&info,UserInfo &ret)
{
    if(info.empty())
        return false;
    memcpy(ret.username,info[0].c_str(),info[0].size());
    memcpy(ret.pwd,info[1].c_str(),info[1].size());
    memcpy(ret.cipher,info[2].c_str(),info[2].size());
    memcpy(ret.isvip,info[3].c_str(),info[3].size());
    memcpy(ret.capacitySum,info[4].c_str(),info[4].size());
    memcpy(ret.usedCapacity,info[5].c_str(),info[5].size());
    memcpy(ret.salt,info[6].c_str(),info[6].size());
    memcpy(ret.vipdate,info[7].c_str(),info[7].size());
    return true;
}


//文件信息体,即保存在数据库中的文件和文件夹
struct FILEINFO{
    std::uint64_t FileId;       //文件在数据库的id
    char Filename[100]={0};     //文件名，最长100字节
    int DirGrade=0;             //目录等级，距离根目录距离，用来构建文件给客户端界面系统
    char FileType[10]={0};      //文件类型，如d为目录，f为文件，也可以拓展MP3等
    std::uint64_t FileSize;     //文件大小
    std::uint64_t ParentDir;    //父文件夹的ID，指明文件保存在哪个文件夹下面
    char FileDate[100]={0};     //文件创建时间
};


//将一个保存文件信息的向量容器里的数据转换为FILEINFO
inline bool VetToFileInfo(const std::vector<std::string>& vet, FILEINFO &fileinfo) {
    if (vet.size() < 6) return false;   // 如果长度对应不上，返回 false

    try {
        // 将字符串转换为 uint64_t
        fileinfo.FileId = std::stoull(vet[0]);  

        // 复制文件名，确保不超过 100 字节
        strncpy(fileinfo.Filename, vet[1].c_str(), sizeof(fileinfo.Filename) - 1);
        fileinfo.Filename[sizeof(fileinfo.Filename) - 1] = '\0'; // 手动添加空终止符
        
        // 转换目录等级
        fileinfo.DirGrade = std::stoi(vet[2]);

        // 复制文件类型，确保不超过 10 字节
        strncpy(fileinfo.FileType, vet[3].c_str(), sizeof(fileinfo.FileType) - 1);
        fileinfo.FileType[sizeof(fileinfo.FileType) - 1] = '\0'; // 手动添加空终止符

        // 将字符串转换为 uint64_t
        fileinfo.FileSize = std::stoull(vet[4]);

        // 将字符串转换为 uint64_t
        fileinfo.ParentDir = std::stoull(vet[5]);

        strncpy(fileinfo.FileDate, vet[6].c_str(), sizeof(fileinfo.FileDate) - 1);
        fileinfo.FileType[sizeof(fileinfo.FileType) - 1] = '\0'; // 手动添加空终止符
        
        return true;
    } catch (const std::exception &e) {
        std::cerr << "VetToFileInfo Error: " << e.what() << std::endl;
        return false;
    }
}

//生成40字节哈希码
inline std::string generateHash(const std::string &username, const std::string &password) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        throw std::runtime_error("无法创建 EVP_MD_CTX");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("初始化 EVP 摘要失败");
    }

    if (EVP_DigestUpdate(mdctx, username.data(), username.size()) != 1 ||
        EVP_DigestUpdate(mdctx, password.data(), password.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("更新 EVP 摘要失败");
    }

    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("最终化 EVP 摘要失败");
    }

    EVP_MD_CTX_free(mdctx);

    // 使用前40个字节
    std::string hash_hex;
    for (unsigned int i = 0; i < 20; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        hash_hex += buf;
    }

    return hash_hex;
}

struct ServerInfoPack{
    char servername[30]={0};
    char ip[20]={0};
    int sport;
    int lport;
    std::uint64_t curConCount=0;
    ServerInfoPack(const string &_ip,const int &_sport,const int &_lport,const string &_servername):sport(_sport),lport(_lport)
    {
        size_t len=_ip.size()>20?20:_ip.size();
        memcpy(ip,_ip.c_str(),len);

        len=_servername.size()>30?30:_servername.size();
        memcpy(servername,_servername.c_str(),len);
    }
    ServerInfoPack()=default;
};

#pragma pack(push, 1)
struct ServerState {
    int code = 0;                     // 状态码，0为正常，1为关闭
    std::uint64_t curConCount = 0;    // 当前连接数
};
#pragma pack(pop)           //禁用内存对齐，方便网络传输

#endif