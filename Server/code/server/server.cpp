#include "server.h"

//初始化服务端函数
Server::Server(const char *host, int port,int upPort, const char *sqlUser, const char *sqlPwd, const char *dbName, 
int connPoolNum, int sqlport, int threadNum, int logqueSize,int _timeout,const char *EqualizerIP,int EqualizerPort,const char *EqualizerKey,const string servername,bool isConEqualizer)
{
   //初始化OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    m_ctx=SSL_CTX_new(TLS_server_method());
    if(!m_ctx)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    //加载服务器的证书和私钥
    if(SSL_CTX_use_certificate_file(m_ctx,"server.crt",SSL_FILETYPE_PEM)<=0||
        SSL_CTX_use_PrivateKey_file(m_ctx,"server.key",SSL_FILETYPE_PEM)<=0){
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
     SSL_CTX_set_verify(m_ctx, SSL_VERIFY_NONE, NULL);   //

    m_epoll=make_unique<Epoller>();    //初始化epfd
    m_workque=make_unique<WorkQue>(threadNum);   //初始化线程池
    m_timer=make_unique<Timer>();               //初始化定时器
    listenEvent_=EPOLLRDHUP|EPOLLET;            //初始化监听套接字为对端挂起和边缘触发
    connEvent_=EPOLLONESHOT | EPOLLRDHUP|EPOLLHUP|EPOLLERR|EPOLLET;   //初始化客户端事件为对端挂起和边缘触发，和需要重用监视

    SqlConnPool::Instance()->Init("localhost",sqlUser,sqlPwd,dbName,connPoolNum);   //初始化连接池
    std::cout<<"连接池已经初始化"<<std::endl;
    Log::Instance()->init(0,LOGPATH,".log",logqueSize); //初始化日记系统
    std::cout<<"日记启动"<<std::endl;
    timeoutMS_=_timeout;                                //初始化超时时间

    tcpInit(host,port,upPort); //初始化监听sockfd;
    std::cout<<"tcp服务已经初始化"<<std::endl;

    //连接负载均衡器
    if(isConEqualizer&&ConnectEqualizer(EqualizerIP,EqualizerPort,host,port,upPort,servername,EqualizerKey))
    {
        EqualizerUsed=true;
        m_epoll->AddFd(SockEqualizer,EPOLLRDHUP|EPOLLHUP|EPOLLERR);
        SetFdNonblock(SockEqualizer);
        std::cout<<"已经连接均衡器"<<std::endl;
    }
}

Server::~Server()
{
    close(sockfd);  //关闭监听套接字
    close(udsockfd);
    SqlConnPool::Instance()->ClosePool();   //关闭连接池
    if(EqualizerUsed){
        sendServerState(1); //发送服务器关闭给均衡器
        close(SockEqualizer);
    }
}

//服务器主循环
void Server::start()
{
    int timeMs=-1;
    while(true)
    {
        if(timeoutMS_>0)    //如果设置超时处理
            timeMs=m_timer->GetNextTick();      //处理超时客户端
        if(EqualizerUsed)
            TimerSendServerState(10000);            //设置间隔10000毫秒和IO事件触发，即10秒发送一次服务器状态信息给均衡器.也可以在连接处理发送，减少性能损耗。
        //开始IO复用 
        int eventcnt=m_epoll->Wait(timeMs); 
        for(int i=0;i<eventcnt;i++)
        {
            int fd=m_epoll->GetEventFd(i);
            uint32_t events=m_epoll->GetEvents(i);
            if(fd==sockfd)
            {
                handleNewConnection(1);     //传递为1，处理短连接
            }
            else if(fd==udsockfd)
            {
                handleNewConnection(2);     //传递为2，处理长连接
            }else if(fd==SockEqualizer&&(events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR)))      //如果是均衡器关闭
            {
                EqualizerUsed=false;
                std::cout<<"均衡器已经关闭"<<std::endl;
                m_epoll->DelFd(fd);
                close(fd);
            }
            else if(events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR))     //如果事件类型是错误、远端关闭挂起、关闭连接等
            {
                assert(m_clientcon.count(fd)>0);
                CloseCon(m_clientcon[fd].get());     //关闭连接
            }else if((events&EPOLLIN)||(events&EPOLLOUT))            //如果是可读可写事件
            {
                assert(m_clientcon.count(fd)>0);
                handleClientTask(m_clientcon[fd].get());             //处理客户端任务
            }else
                LOG_ERROR("unexpected events");
        }
    }
}

// 初始化监听套接字
int Server::tcpInit(const char * ip, int port,int updownPort)
{
    //创建套接字
    int ret=0;

    if(port>65535||port<1024||updownPort>65535||updownPort<1024){
        LOG_ERROR("port:%derror",port);
        return false;
    }

    sockfd=socket(AF_INET,SOCK_STREAM,0);       //处理客户端短任务套接字
    udsockfd=socket(AF_INET,SOCK_STREAM,0);     //处理客户端长任务套接字
    ERROR_CHECK(udsockfd,-1,"socket");
    ERROR_CHECK(sockfd,-1,"socket");

    //设置套接字选项
    int on=1;
    struct linger optLinger = { 0 };
    optLinger.l_linger=1;
    optLinger.l_onoff=1;
    ret=setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));  //设置接受端口重用
    ERROR_CHECK(ret,-1,"setsockopt");
    ret=setsockopt(udsockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));  //设置接受端口重用
    ERROR_CHECK(ret,-1,"setsockopt");
    ret = setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger)); //设置套接字优雅关闭
    ERROR_CHECK(ret,-1,"setsockopt");
    ret = setsockopt(udsockfd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger)); //设置套接字优雅关闭
    ERROR_CHECK(ret,-1,"setsockopt");


    //绑定套接字
    struct sockaddr_in sockinfo;
    bzero(&sockinfo,sizeof(sockinfo));
    sockinfo.sin_addr.s_addr=inet_addr(ip);
    sockinfo.sin_family=AF_INET;
    sockinfo.sin_port=htons(port);

    ret=bind(sockfd,(sockaddr *)&sockinfo,sizeof(sockinfo));
    ERROR_CHECK(ret,-1,"bind");

    struct sockaddr_in updpwninfo=sockinfo;
    sockinfo.sin_port=htons(updownPort);
    ret=bind(udsockfd,(sockaddr *)&sockinfo,sizeof(updpwninfo));
    ERROR_CHECK(ret,-1,"bind");

    //开始监听
    ret=listen(sockfd,10);
    ERROR_CHECK(ret,-1,"listen");

    ret=listen(udsockfd,10);
    ERROR_CHECK(ret,-1,"listen");

    SetFdNonblock(sockfd);                              //设置套接字非阻塞
    SetFdNonblock(udsockfd);                              //设置套接字非阻塞
    m_epoll->AddFd(sockfd,listenEvent_|EPOLLIN);        //将套接字添加到epoll监控,监控默认事件和可读事件
    m_epoll->AddFd(udsockfd,listenEvent_|EPOLLIN);        //将套接字添加到epoll监控,监控默认事件和可读事件
    return true;
}

//关闭超时、异常的客户端连接
void Server::CloseCon(AbstractCon *client)
{
    assert(client);
    LOG_INFO("Client[%d] quit",client->getsock());
    m_epoll->DelFd(client->getsock());  //删除监听描述符
    client->Close();      
}

//使用边缘触发，持续处理新连接，直到无新连接
void Server::handleNewConnection(int select)
{
    struct sockaddr_in addr;
    socklen_t len=sizeof(addr);
    int acceptfd=select==1?sockfd:udsockfd;     //根据传递值，不同决定接受套接字
    do{
        int fd=accept(acceptfd,(struct sockaddr *)&addr,&len);
        if(fd<=0)break;    //已经无新连接
        else if(AbstractCon::userCount>=MAX_FD)   //超出最大连接数
        {
            SendError(fd,"Server busy");
            LOG_WARN("Connect is full");
            return;
        }
        //进行ssl握手
        SSL *ssl=SSL_new(m_ctx);
        SSL_set_fd(ssl,fd);
        if(SSL_accept(ssl)<=0)
        {
            LOG_ERROR("ssl connection fail:%d",fd);
            SSL_free(ssl);
            SendError(fd,"ssl connection fail");
            close(fd);
            continue;
        }
        AddClient(fd,ssl,select);
    }while(true);
}


//处理客户端发来的任务
void Server::handleClientTask(AbstractCon *client)
{
    assert(client);
    if(timeoutMS_>0)//延长超时时间
        m_timer->adjust(client->getsock(),timeoutMS_); 
    m_workque->AddTask(std::bind(&Server::createFactory,this,client));  //往线程池添加任务，调用createFactory
}

//接收客户端命令，生成具体工厂分类处理任务
void Server::createFactory(AbstractCon *con)
{
    std::shared_ptr<AbstractTool>Tool;
    SR_Tool sr_tool;            //创建一个发收工具
    if(con->ClientType==AbstractCon::ConType::SHOTTASK) //如果是短任务
    {
        PDU pdu;    //通信包体
        sr_tool.recvPDU(con->getSSL(),pdu);
        Tool=GetTool(pdu,con);
    }
    else if((con->ClientType==AbstractCon::ConType::LONGTASK&&con->getIsVerify()==false)) //长任务已经首次连接，无法确定是下载还是上传）
    {
        TranPdu pdu;
        sr_tool.recvTranPdu(con->getSSL(),pdu);
        Tool=GetTool(pdu,con);
    }
    else       //长任务已经认证，处于进行中状态
    {
        if(con->ClientType==AbstractCon::ConType::PUTTASK)  //上传任务
            Tool=std::make_shared<PutsTool>(con);
        if(con->ClientType==AbstractCon::ConType::GETTASK)  //下载任务
            Tool=std::make_shared<GetsTool>(con);
        if(con->ClientType==AbstractCon::ConType::GETTASKWAITCHECK)  //下载任务完成等待确认
            Tool=std::make_shared<GetsTool>(con);
    }
    if(Tool)    
        Tool->doingTask();  //如果不为空开始处理任务

     //重新添加监控
    if(con->ClientType==AbstractCon::ConType::GETTASK){  //如果是下载任务，需要多监控可写事件
        m_epoll->ModFd(con->getsock(),connEvent_|EPOLLIN|EPOLLOUT);  
    }
    else  
        m_epoll->ModFd(con->getsock(),connEvent_|EPOLLIN);      //重新添加监控
    
}

//根据任务代码不同，生成不同的具体工厂类处理短任务
std::shared_ptr<AbstractTool> Server::GetTool(PDU &pdu, AbstractCon *con)
{
    assert(con);

    switch(pdu.PDUcode)   
    {
        case Code::LogIn:
        {
            return std::make_shared<LoginTool>(pdu,con);
            break;
        }
        case Code::SignIn:
        {
            return std::make_shared<SignTool>(pdu,con);
            break;
        }
        case Code::CD:
        {
            return std::make_shared<CdTool>(pdu,con);
            break;
        }
        case Code::MAKEDIR:
        {
            return std::make_shared<CreateDirTool>(pdu,con);
            break;
        }
        case Code::DELETEFILE:
        {
            return std::make_shared<DeleteTool>(pdu,con);
            break;
        }
        default:
        break; 
    }
    return std::shared_ptr<AbstractTool>(nullptr);  //返回空指针
}

//根据任务代码不同，生成不同的具体工厂类处理长任务
std::shared_ptr<AbstractTool> Server::GetTool(TranPdu &pdu, AbstractCon *con)
{
    assert(con);

    switch(pdu.TranPduCode)
    {
        case Code::Puts:
        {
            con->ClientType=AbstractCon::ConType::PUTTASK;  //此时任务类型已经可以确定
            return std::make_shared<PutsTool>(pdu,con);            
        }
        case Code::PutsContinue:
        {
            con->ClientType=AbstractCon::ConType::PUTTASK;  //此时任务类型已经可以确定
            return std::make_shared<PutsTool>(pdu,con);            
        }
        case Code::Gets:
        {
            con->ClientType=AbstractCon::ConType::GETTASK; //确定为下载任务
            return std::make_shared<GetsTool>(pdu,con);
        }
        default:
            break;
    }
    return std::shared_ptr<AbstractTool>(nullptr);  //返回空指针
}

//连接负载均衡器，返回是否连接成功
bool Server::ConnectEqualizer(const string &EqualizerIP, const int &EqualizerPort, const string &MineIP, const int MiniSPort, const int &MiniLPort,const string &servername,const string &key)
{
    int ret=0;
    ServerInfoPack pack(MineIP,MiniSPort,MiniLPort,servername);
    SockEqualizer = socket(AF_INET, SOCK_STREAM, 0);
    if (SockEqualizer < 0) {
        std::cerr << "Failed to create server socket." << std::endl;
        return false;
    }

    // 设置套接字选项，允许地址重用
    int opt = 1;
    if (setsockopt(SockEqualizer, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options: " << strerror(errno) << std::endl;
        close(SockEqualizer);
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(EqualizerIP.c_str());
    server_addr.sin_port = htons(EqualizerPort);

    if (connect(SockEqualizer, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to connect to the equalizer." << std::endl;
        close(SockEqualizer);
        return false;
    }
    char buf[60]={0};
    memcpy(buf,key.c_str(),key.size());
    ret=write(SockEqualizer,pack.servername,sizeof(pack.servername));
    ret=write(SockEqualizer, pack.ip, sizeof(pack.ip));
    ret=write(SockEqualizer, &pack.sport, sizeof(pack.sport));
    ret=write(SockEqualizer, &pack.lport, sizeof(pack.lport));
    ret=write(SockEqualizer, &pack.curConCount, sizeof(pack.curConCount));
    ret=write(SockEqualizer, buf, sizeof(buf));

    bool res;
    ret=read(SockEqualizer,&res,sizeof(res));
    return res;
}

//向均衡器发送状态信息
void Server::sendServerState(int state)
{
    ServerState cur;
    cur.code=state;
    cur.curConCount=AbstractCon::userCount;     //当前用户数
    send(SockEqualizer,&cur,sizeof(cur),0);
}

//间隔millisecond毫秒向负载均衡器发送状态信息
void Server::TimerSendServerState(int millisecond)
{
    static auto lastTime = std::chrono::high_resolution_clock::now(); // 使用高分辨率时钟.静态对象，只初始化一次，下次进入不会初始化

    // 获取当前时间
    auto currentTime = std::chrono::high_resolution_clock::now();
    // 计算时间差
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);

    // 检查是否超过设定的时间间隔
    if (duration.count() > millisecond) {
        sendServerState(0); // 向均衡器发送信息
        lastTime = currentTime; // 更新最后发送时间
    }
}

// 设置套接字非阻塞
int Server::SetFdNonblock(int fd)
{
    assert(fd>0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

void Server::SendError(int fd, const char *info)
{
    ssize_t bytes=write(fd,info,sizeof(info));
    if(bytes==-1)
        LOG_ERROR("send error failed");
}

//添加新客户端
void Server::AddClient(int clientfd, SSL *ssl,int select)
{
    assert(clientfd>0);
    assert(ssl!=nullptr);
    if(select==1){   //如果是短任务连接
        m_clientcon[clientfd]=std::make_shared<ClientCon>(clientfd,ssl);  //生成一个连接对象，添加进哈希映射
        m_clientcon[clientfd]->ClientType=AbstractCon::ConType::SHOTTASK;   //短任务
    }
    else {           //如果是长任务连接
        m_clientcon[clientfd]=std::make_shared<UpDownCon>(clientfd,ssl);
        m_clientcon[clientfd]->ClientType=AbstractCon::ConType::LONGTASK;
    }
    if(timeoutMS_>0)    
        m_timer->add(clientfd,timeoutMS_,std::bind(&Server::CloseCon,this,m_clientcon[clientfd].get()));
    SetFdNonblock(clientfd);                        //设置套接字非阻塞
    m_epoll->AddFd(clientfd,EPOLLIN|connEvent_);    //添加进epoll监控
    LOG_INFO("client[%d] in",clientfd);             //记录日记
}
