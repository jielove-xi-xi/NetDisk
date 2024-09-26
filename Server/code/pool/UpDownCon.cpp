#include "UpDownCon.h"


//构造函数，传递套接字和安全套接字构造
UpDownCon::UpDownCon(int sockFd, SSL *_ssl)
{
    userCount++;
    m_clientsock=sockFd;
    m_clientSsl=_ssl;
}

//析构函数关闭任务
UpDownCon::~UpDownCon()
{
    Close();
}

//重载=赋值运算符
UpDownCon &UpDownCon::operator=(const UpDownCon &other)
{
    if (this == &other) {
        return *this;  // 防止自赋值
    }

    // 复制基本成员
    m_status.store(other.m_status.load());  // 原子类型需要特殊处理
    m_task = other.m_task;  // 任务结构体可以直接赋值

    return *this;
}

// 初始化任务类
void UpDownCon::init(const UserInfo &info, const UDtask &task)
{
    m_userinfo=info;
    m_task=task;
    m_isVip=(string(m_userinfo.isvip)=="1");
}

void UpDownCon::Close()
{
    if(m_isClose==true)return;
    m_status=UpDownCon::COLSE;        //修改状态，避免线程继续处理
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
        else
            std::cout<<"客户端关闭ssl成功"<<std::endl;
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

    // 清理内存映射
    if (m_task.FileMap && m_task.total > 0)
    {
        if (munmap(m_task.FileMap, m_task.total) != 0)
        {
            std::cerr << "munmap failed: " << strerror(errno) << std::endl;
        }
        m_task.FileMap = nullptr; // 防止重复释放
    }
    if (m_task.File_fd >= 0)
    {
        if (close(m_task.File_fd) < 0)
        {
            std::cerr << "Failed to close file descriptor: " << strerror(errno) << std::endl;
        }
        m_task.File_fd = -1;
    }
    userCount--;
    m_isClose=true;
    
    //如果不需要实现断点上传，可以将未完成的任务文件直接删除，避免服务器磁盘空间浪费。如果需要实现，这段代码需要修改，可以使用数据库记录保存客户端上传传输任务状态
    if(m_task.handled_Size<m_task.total&&m_task.TaskType==UpDownCon::ConType::PUTTASK)    //任务未完成，且为上传任务
    {
        std::string filepath=string(ROOTFILEPATH)+"/"+string(m_userinfo.username)+"/"+m_task.FileMD5;    //文件名为，用户根文件夹+/+文件md5码
        if(remove(filepath.c_str())==0)
        {
            LOG_INFO("Remove file:%s",filepath.c_str());
        }
        else{
            LOG_INFO("Remove file fail:%s",filepath.c_str());
        }
    }
}

// 设置任务状态
void UpDownCon::SetStatus(UDStatus status)
{
    m_status.store(status);                                        //修改状态
}

// 返回任务结构体指针
UDtask *UpDownCon::GetUDtask() 
{
    return &m_task;
}

//返回任务的当前状态
int UpDownCon::GetStatus() 
{
    return m_status;
}
