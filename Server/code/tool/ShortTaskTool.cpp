#include"ShortTaskTool.h"



//登陆操作
int LoginTool::doingTask()
{
    //查找用户信息，确定用户存在
    ResponsePack respon;
    UserInfo info;
    MyDb db;
    ClientCon *m_con=dynamic_cast<ClientCon*>(m_conParent);
    bool status=db.getUserInfo(m_pdu.user,m_pdu.pwd,info);
    if(status&&string(info.cipher)!="")
    {
        respon.code=Code::CodeOK;   //返回正确
        m_con->init(info);      //保存客户信息
        m_con->setVerify(true); //设置客户端已经通过认证
    }else
        respon.code=Code::CodeNO;   //返回错误
    sr_tool.sendReponse(m_con->getSSL(),respon);  //发送回客户端 
    if(respon.code==Code::CodeOK)
    {
        sr_tool.sendUserInfo(m_con->getSSL(),info);     //  发送客户信息
        LOG_INFO("User:%s Login",m_pdu.user);
        return 0;
    }
    else 
        return -1;
}


//注册用户
int SignTool::doingTask()
{
    ResponsePack respon;    //回复体
    UserInfo info;          //客户信息
    MyDb db;                //数据库连接
    ClientCon *m_con=dynamic_cast<ClientCon*>(m_conParent);
    bool status=db.getUserExist(m_pdu.user);    //查询是否存在该用户
    if(status==true)                            //如果不存在
    {
        std::string ciper=generateHash(m_pdu.user,m_pdu.pwd);       //生成一个哈希密文
        status=db.insertUser(m_pdu.user,m_pdu.pwd,ciper);           //插入用户，其它字段为默认值
        if(status)
            status=createDir();     //创建用户根文件夹
        if(status)
        {
            respon.code=Code::CodeOK;    
            db.getUserInfo(m_pdu.user,m_pdu.pwd,info);      //获取用户信息
            m_con->init(info);          //用客户信息保存在连接类中
            m_con->setVerify(true);     //标志为已经通过认证客户端
        }else
            respon.code=Code::CodeNO;
    }
    else
    {
        respon.code=Code::CodeNO;
    }
    sr_tool.sendReponse(m_con->getSSL(),respon);
    if(respon.code==Code::CodeOK)
    {
        sr_tool.sendUserInfo(m_con->getSSL(),info);
        LOG_INFO("User:%s Sgin",m_pdu.user);
        return 0;
    }else
        return -1;
}

//创建用户根文件夹
bool SignTool::createDir()
{
    char temp[1024] = {0};
    std::string path;
    mode_t mode = 0755;  // 文件夹的权限

    // 获取当前工作目录
    if (getcwd(temp, sizeof(temp)) != NULL)
        path = std::string(temp);
    else
        return false;

    // 将用户名称拼接到路径后
    path += "/"+string(ROOTFILEPATH)+("/" + std::string(m_pdu.user));

    struct stat info;


    // 检查文件/文件夹是否存在
    if (stat(path.c_str(), &info) != 0) {
        // 文件不存在，尝试创建文件夹
        int result = mkdir(path.c_str(), mode);
        if (result == 0) {
            return true;
        } else {
            LOG_INFO("mkdir error %s",m_pdu.user);
            return false;
        }
    } 
    else if (info.st_mode & S_IFDIR) 
    {
         return true;  // 如果已经存在，可以共有，返回true
    } else 
    {
        LOG_INFO("mkdir error %s",m_pdu.user);
        return false;
    }
}

//执行向客户端传输文件信息任务
int CdTool::doingTask()
{
    ResponsePack respon;
    ClientCon *m_con=dynamic_cast<ClientCon*>(m_conParent);
    SSL *ssl=m_con->getSSL();
    if(!m_con->getIsVerify())  //如果客户端没认证
    {
        respon.code=Code::LogIn;    //返回告诉客户端先进行登陆操作
        sr_tool.sendReponse(ssl,respon);
        return -1;
    }
    MyDb db;
    vector<FILEINFO>filevet;
    bool status=db.getUserAllFileinfo(m_con->getuser(),filevet);
    if(!status)
    {
        respon.code=Code::CodeNO;   //发送错误回去给客户端
        sr_tool.sendReponse(ssl,respon);
        return -1;
    }

    //正常发送全部文件信息
    size_t count=filevet.size();

    //将回复体发送回客户端
    respon.code=Code::CodeOK;   
    respon.len=sizeof(std::size_t);
    memcpy(respon.buf,&count,sizeof(std::size_t));
    sr_tool.sendReponse(ssl,respon);

    //开始逐一发送
    sr_tool.sendFileInfo(ssl,filevet);
    LOG_INFO("client %s cd",m_con->getuser().c_str());
    return 0;
}


//执行创建文件夹任务
int CreateDirTool::doingTask()
{
    ResponsePack respon;
    SSL *ssl = m_con->getSSL();

    // 如果客户端未认证
    if (!m_con->getIsVerify()) {
        respon.code = Code::LogIn;    // 返回登录提示
        sr_tool.sendReponse(ssl, respon);
        return -1;
    }
    // 设置数据库及任务信息
    MyDb db;
    UDtask task;
    task.Filename = m_pdu.filename;

    memcpy(&task.parentDirID,m_pdu.reserve,sizeof(std::uint64_t));
    // 插入文件夹数据
    uint64_t newid = db.insertFiledata(m_con->getuser(), &task, "d");
    if (newid != 0) {
        respon.code = Code::CodeOK;
        respon.len = sizeof(uint64_t);
        memcpy(respon.buf, &newid, sizeof(uint64_t));  // 传递新创建文件夹的 ID
    } else {
        respon.code = Code::CodeNO;  // 返回数据库插入失败的错误码
    }

    // 发送响应
    sr_tool.sendReponse(ssl, respon);
    LOG_INFO("client %s created directory: %s", m_con->getuser().c_str(), task.Filename.c_str());
    return 0;
}


//删除文件或者文件夹
int DeleteTool::doingTask()
{
    ResponsePack respon;
    SSL *ssl = m_con->getSSL();

    // 检查 SSL 连接是否有效
    if (ssl == nullptr) {
        std::cerr << "SSL connection is not initialized." << std::endl;
        return -1;
    }

    // 如果客户端未认证
    if (!m_con->getIsVerify()) {
        respon.code = Code::LogIn; // 返回登录提示
        sr_tool.sendReponse(ssl, respon);
        return -1;
    }

    MyDb db;
    string suffix = getSuffix(m_pdu.filename); // 获取后缀名
    bool ret = false;
    std::uint64_t fileid = 0;

    // 确保 m_pdu.pwd 有效，安全地拷贝文件 ID
    if (sizeof(m_pdu.pwd) < sizeof(std::uint64_t)) {
        std::cerr << "Invalid password data length." << std::endl;
        respon.code = Code::CodeNO;
        sr_tool.sendReponse(ssl, respon);
        return -1;
    }
    memcpy(&fileid, m_pdu.pwd, sizeof(std::uint64_t));

    // 根据文件类型删除文件或文件夹
    if (suffix == "d") { // 如果类型是文件夹
        ret = db.deleteOneDir(m_con->getuser(), fileid);
    } else {
        ret = db.deleteOneFile(m_con->getuser(), fileid);
    }

    // 设置响应代码
    if (ret) {
        respon.code = Code::CodeOK;
    } else {
        respon.code = Code::CodeNO;
        std::cerr << "Failed to delete " << (suffix == "d" ? "directory" : "file") << " with ID: " << fileid << std::endl;
    }

    // 发送响应
    sr_tool.sendReponse(ssl, respon);
    return 0;
}
