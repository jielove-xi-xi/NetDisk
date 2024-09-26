#include"LongTaskTool.h"


//上传操作
int PutsTool::doingTask()
{
    //状态机转换执行
    UpDownCon *m_con=dynamic_cast<UpDownCon*>(m_conParent); //转换成子类对象
    if(m_con->GetStatus()==UpDownCon::UDStatus::START)      //如果刚刚开始，没有进行连接和认证
    {
        FirstCheck(m_con);
    }
    if(m_con->GetStatus()==UpDownCon::UDStatus::DOING)    //如果处于进行中状态，则接受数据。
    {
        RecvFileData(m_con);
    }

    if(m_con->GetStatus()==UpDownCon::FIN)
    {
        MyDb db;
        UDtask *task=m_con->GetUDtask();
        string suffix=getSuffix(task->Filename);
        std::uint64_t ret=db.insertFiledata(m_con->getuser(),task,suffix); //在数据库中插入该记录并更新已经使用空间
        ResponsePack res;
        if(ret!=0)
            res.code=Code::CodeOK;
        else
            res.code=Code::CodeNO;
        if(ret!=0)      //如果得到ID，返回给客户端
        {
            res.len=sizeof(std::uint64_t);
            memcpy(res.buf,&ret,sizeof(std::uint64_t));      
        }
        sr_tool.sendReponse(m_con->getSSL(),res);   //发送最后成功指令
        m_con->closessl();
        m_con->SetStatus(UpDownCon::COLSE); //设置为关闭状态
        LOG_INFO("client %s puts:%s",m_con->getuser().c_str(),m_con->GetUDtask()->Filename.c_str());
    }
    else if(m_con->GetStatus()==UpDownCon::COLSE)    //关闭状态
    {
        m_con->Close();
    }

    return 0;
}

//初始认证
bool PutsTool::FirstCheck(UpDownCon *m_con)
{
    MyDb db;
    ResponsePack respon;
    UserInfo info;
    bool status=db.getUserInfo(m_pdu.user,m_pdu.pwd,info);

    if(status&&string(info.cipher)!="")  //客户端发送过来的用户名和密码通过认证
    {
        UDtask task=CreateTask(respon,db);
        if(respon.code==Code::CodeOK||respon.code==Code::PutsContinueNO||respon.code==Code::PutQUICKOK)
        {
            m_con->init(info,task);      //保存客户信息,初始化连接类
            m_con->setVerify(true); //设置客户端已经通过认证
            if(respon.code==Code::PutQUICKOK)   //如果可以妙传
                m_con->SetStatus(UpDownCon::UDStatus::FIN); //改成完成状态
            else
                m_con->SetStatus(UpDownCon::UDStatus::DOING);   //把状态改成进行中
        }
    }else
        respon.code=Code::CodeNO;
    
    sr_tool.sendReponse(m_con->getSSL(),respon);    //将结果发回客户端
    if(respon.code==Code::CodeNO||respon.code==Code::CapacityNO)   //出错改成从新认证
    {
        m_con->ClientType=AbstractCon::LONGTASK;
        m_con->setVerify(false);
    }

    return true;
}

//创建任务结构
UDtask PutsTool::CreateTask(ResponsePack &respon,MyDb &db)
{
    UDtask task;
    task.TaskType=UpDownCon::ConType::PUTTASK;  //任务类型为上传
    task.Filename=m_pdu.filename;   //任务文件名
    task.FileMD5=string(m_pdu.FileMd5);     //文件MD5码,加上用户根文件夹，用户名就是根文件夹名，保证用户名唯一，所以文件夹唯一
    task.total=m_pdu.filesize;      //文件总大小
    task.parentDirID=m_pdu.parentDirID; //父文件夹ID

    string suffix=getSuffix(m_pdu.filename);    //后缀名检查
    if(suffix=="d")     //后缀名不合法
    {
        respon.code=CodeNO;
        return task;
    }
    
    if(!db.GetISenoughSpace(m_pdu.user,m_pdu.pwd,m_pdu.filesize))   //如果空间不足
    {
        respon.code=Code::CapacityNO;
        return task;
    }

    if(db.getFileExist(m_pdu.user,m_pdu.FileMd5))   //如果文件已经存在，支持妙传
    {
        respon.code=Code::PutQUICKOK;
        task.handled_Size=task.total;       //修改已经处理大小为总大小，否则会导致文件被删除
        return task;                        //返回即可，后面无需操作
    }

    respon.code=Code::CodeOK; //暂时默认为OK
    struct stat isExist;
    string openfilename=string(ROOTFILEPATH)+"/"+string(m_pdu.user)+"/"+string(m_pdu.FileMd5);
    if(m_pdu.TranPduCode==Code::PutsContinue&&(stat(openfilename.c_str(),&isExist)==0))  //如果客户端要求断点续传，且文件存在
    {
        task.handled_Size=isExist.st_size;            //将断点续传位置定义为文件大小
        respon.len=sizeof(std::uint64_t);             //保存已经上传位置到回复体，告诉客户端从哪里开始传输
        memcpy(respon.buf,&task.handled_Size,sizeof(std::uint64_t));
    }else if(m_pdu.TranPduCode==Code::PutsContinue) //要求断点续传，文件不存在
    {
        respon.code=Code::PutsContinueNO;   //从零开始
        task.handled_Size=0;
    }

    int file_fd=open(openfilename.c_str(),O_RDWR|O_CREAT,0666);  //打开文件
    if(file_fd==-1)
    {
        perror("open file in puts");
        respon.code=Code::CodeNO;
        return task;
    }

    int ret=ftruncate(file_fd, task.total);
    if (ret == -1) {
        perror("ftruncate");
        close(file_fd);
        respon.code = Code::CodeNO;
        return task;
    }

    char *pmap=(char *)mmap(NULL,task.total,PROT_WRITE,MAP_SHARED,file_fd,0);   //偏移量从0开始
    
    if(pmap==(char*)-1)
    {
        perror("mmap");
        close(file_fd);
        respon.code=Code::CodeNO;
        return task;
    }

    task.File_fd=file_fd;
    task.FileMap=pmap;
    
    return task;
}

//接受客户端的数据
void PutsTool::RecvFileData(UpDownCon *m_con)
{
    // 设置每次接收的数据大小
    const size_t bufferSize = 4096;

    ssize_t len = -1;
    UDtask *task = m_con->GetUDtask();
    size_t total = task->total;
    SSL *ssl = m_con->getSSL();

    if (ssl == nullptr) {
        m_con->SetStatus(UpDownCon::COLSE); // 设置为关闭状态
        return;
    }

    size_t remaining = total - task->handled_Size;
    size_t bytes = remaining > bufferSize ? bufferSize : remaining;

    len = SSL_read(ssl, task->FileMap + task->handled_Size, bytes); //直接往内存映射写数据
    if (len > 0) 
        task->handled_Size += len; // 更新已处理字节数
    
    //能接受多少就接受多少
    // do {
    //     // 计算剩余未处理的数据大小
    //     size_t remaining = total - task->handled_Size;
    //     size_t bytes = remaining > bufferSize ? bufferSize : remaining;
    //     if(ssl==nullptr)
    //     {
    //         m_con->SetStatus(UpDownCon::COLSE);//设置为关闭状态
    //         break;
    //     }
    //     len = SSL_read(ssl, task->FileMap+task->handled_Size, bytes);
    //     if (len > 0) {
    //         task->handled_Size += len;          // 更新偏移量
    //     } else if (len <= 0 || task->handled_Size == task->total) {
    //         // 文件处理完毕或当前没有数据可读
    //         break;
    //     }
    // } while (true);

    // 更新状态机
    if (task->handled_Size == total) {
        m_con->SetStatus(UpDownCon::FIN); // 改成完成状态
    }
}


//================================================GetTool类====================================================

// 执行下载任务的主函数
int GetsTool::doingTask() {
    if (m_con->GetStatus() == UpDownCon::UDStatus::START) {
        FirstCheck();  // 首次连接认证
    } 
    if (m_con->GetStatus() == UpDownCon::UDStatus::DOING) {
        SendFileData();  // 发送文件数据
        if (m_con->GetStatus()==UpDownCon::UDStatus::FIN)
             m_con->ClientType=AbstractCon::GETTASKWAITCHECK;   //更改连接类型为等待确认，只监听可读事件，不再监听可写事件  
    }

    if (m_con->GetStatus() == UpDownCon::FIN) {
        bool isOK=false;
        int bytes=SSL_read(m_con->getSSL(),&isOK,sizeof(bool));
        if(isOK==true)                                          //接受客户端回复，客户端SHA256检查成功。
        {
            m_con->closessl();
            m_con->SetStatus(UpDownCon::COLSE);
            LOG_INFO("client %s gets: %s", m_con->getuser().c_str(), m_con->GetUDtask()->Filename.c_str());
        }else if(bytes==1&&isOK!=true)                          //可以在这里实现检验出错，处理客户重传，重新把状态机改成DOING。连接类型重新设置为GETS暂不实现
        {                       
            m_con->closessl();
            m_con->SetStatus(UpDownCon::COLSE);
            LOG_INFO("client %s gets failed: %s", m_con->getuser().c_str(), m_con->GetUDtask()->Filename.c_str());
        }
    }
    else if (m_con->GetStatus() == UpDownCon::COLSE) {
        m_con->Close();  // 关闭连接
    }
    return 0;
}


// 首次连接认证函数
bool GetsTool::FirstCheck() {
    MyDb db;
    ResponsePack respon;
    UserInfo info;
    bool status = db.getUserInfo(m_pdu.user, m_pdu.pwd, info);

    if (status && string(info.cipher) != "") {  // 用户名密码认证通过
        UDtask task;
        if (CreateTask(respon, task, db)) {
            m_con->init(info, task);
            m_con->setVerify(true);
            respon.len=task.FileMD5.size();     //将md5发送回给客户端，方便客户端下载完成后进行检查，是否正常
            memcpy(respon.buf,task.FileMD5.c_str(),task.FileMD5.size());
            m_con->SetStatus(UpDownCon::UDStatus::DOING);  // 设置为进行中状态
        }
    } else {
        respon.code = Code::CodeNO;
    }
    sr_tool.sendReponse(m_con->getSSL(), respon);  // 发送认证结果
    if (respon.code !=Code::CodeOK) {
        m_con->ClientType = AbstractCon::LONGTASK;
        m_con->setVerify(false);  // 重新认证
    }

    return true;
}

// 创建下载任务函数
bool GetsTool::CreateTask(ResponsePack &respon, UDtask &task, MyDb &db) {

    task.TaskType = UpDownCon::ConType::GETTASK;    // 设置为下载任务
    task.Filename = m_pdu.filename;                 // 任务文件名

    //查询文件是否存在，并返回MD5码
    if(!db.GetFileMd5(m_pdu.user,m_pdu.parentDirID,task.FileMD5))   //查询文件MD5码，不存在返回false，parentDirID指的是要下载的文件ID
    {
        respon.code=Code::FileNOExist;
        return false;
    }

    string fullPath = string(ROOTFILEPATH)+"/"+string(m_pdu.user) + "/" +task.FileMD5;  // 用户目录下的文件路径
    struct stat fileStat;

    if (stat(fullPath.c_str(), &fileStat) == -1) {  // 文件不存在
        respon.code = Code::FileNOExist;
        return false;
    }

    task.total = fileStat.st_size;  // 文件总大小

    //断点续传或者从新开始从传
    if(m_pdu.sendedsize<task.total)
        task.handled_Size = m_pdu.sendedsize;       //客户端指定的偏移量，但是不能大于文件总字节.默认为0
    else
    {
        respon.code=Code::GetsContinueNO;           //断点下载失败
        return false;
    }

    // 打开文件以便读取
    int file_fd = open(fullPath.c_str(), O_RDONLY);
    if (file_fd == -1) {
        perror("open file in gets");
        respon.code = Code::CodeNO;
        return false;
    }

    char *pmap = (char *)mmap(NULL, task.total, PROT_READ, MAP_SHARED, file_fd, 0);  // 内存映射
    if (pmap == (char *)-1) {
        perror("mmap");
        close(file_fd);
        respon.code = Code::CodeNO;
        return false;
    }

    task.File_fd = file_fd;
    task.FileMap = pmap;

    respon.code = Code::CodeOK;  // 设置为OK
    return true;
}

//轮询发送文件数据
void GetsTool::SendFileData() {
    size_t chunkSize = 2048;        // 每次发送的块大小，2KB,避免线程被一个任务占用太长时间
    if(m_con->getisVip())           //如果是会员,多发送一些
        chunkSize*=3;

    UDtask *task = m_con->GetUDtask();
    size_t total = task->total;
    SSL *ssl = m_con->getSSL();

    if (ssl == nullptr) {
        m_con->SetStatus(UpDownCon::COLSE);
        return;
    }

    // 每次仅发送部分数据
    size_t remaining = total - task->handled_Size;                          //总字节减去已经处理字节，剩余字节
    size_t bytesToSend = remaining > chunkSize ? chunkSize : remaining;     //待发送字节数

    const char * dataToSend=task->FileMap+task->handled_Size;               //确定偏移量
    
    // 通过 SSL 发送数据
    ssize_t sentBytes = SSL_write(ssl, dataToSend, bytesToSend);         //直接使用偏移量发送，避免拷贝
 
    if (sentBytes > 0) {
        // 更新已处理的字节数
        task->handled_Size += sentBytes;
    }

    // 检查文件是否已经全部发送
    if (task->handled_Size == total) {
        m_con->SetStatus(UpDownCon::FIN);  // 文件传输完成
    } else if (sentBytes <= 0) {
        // 如果发送失败或者出现问题，设置为关闭状态
        m_con->SetStatus(UpDownCon::COLSE);
    }
}