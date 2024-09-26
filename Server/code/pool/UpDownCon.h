#ifndef UPDOWNCON_H
#define UPDOWNCON_H

#include"AbstractCon.h"



//上传和下载任务结构体
struct UDtask{
    int TaskType=AbstractCon::ConType::LONGTASK;                       //任务类型，默认为长任务，后期再改上传或者下载
    std::string Filename;               //文件名
    std::string FileMD5;                //文件存储在磁盘中的MD5码
    std::uint64_t total=0;              //文件总字节大小
    std::uint64_t handled_Size=0;       //已经处理大小
    std::uint64_t parentDirID=0;        //保存在哪个文件夹下，默认为0根目录
    int File_fd=-1;                     //文件套接字
    char *FileMap=nullptr;              //文件内存映射mmap
};



//该类专门负责处理长时间耗时操作的连接，如上传文件和下载文件,并用原子类型保证线程安全
class UpDownCon:public AbstractCon{
public:
    UpDownCon(int sockFd,SSL *_ssl);            
    UpDownCon()=default;
    ~UpDownCon();

    UpDownCon& operator=(const UpDownCon &other);  // 手动实现赋值运算符

    void init(const UserInfo &info,const UDtask &task);     //初始化任务
    void Close();                                           //关闭连接

    enum UDStatus{START=0,DOING,PAUSE,FIN,COLSE};     //状态机状态枚举变量，分别为：开始、上传下载中、暂停，完成,关闭
    void SetStatus(UDStatus status);                       //修改当前任务状态
    UDtask* GetUDtask();                               //获取任务结构体指针
    int GetStatus();                                        //返回当前任务状态

private:
    std::atomic<int> m_status{0};                               //状态变量
    UDtask m_task;                                //任务体
};




#endif