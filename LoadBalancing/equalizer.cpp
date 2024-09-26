#include "equalizer.h"

Equalizer::Equalizer(std::string ip,int Port_server,int Port_Client,string key)
{
    if(tcpInit(ip,Port_server,Port_Client)){
        std::cout<<"均衡器已经启动"<<std::endl;
        std::cout<<"开始监听地址："<<ip<<" 服务器交互端口："<<Port_server<<" 客户端交互端口"<<Port_Client<<std::endl;
    }
    else
        std::cout<<"均衡器初始化出错"<<std::endl;
    ConKey=key;
}

// 析构函数
Equalizer::~Equalizer()
{
    m_serverHeap.clear();
}

// 均衡器主循环
void Equalizer::start()
{
    while(true)
    { 
        //开始IO复用

        displayServerInfo();        //仅用于调试，正式需要删除，否则浪费性能
        int eventcnt=m_epoll.Wait();
        for(int i=0;i<eventcnt;i++)
        {
            int fd=m_epoll.GetEventFd(i);
            uint32_t events=m_epoll.GetEvents(i);
            if(fd==m_sockAboutServer)      //如果是服务端监听套接字
                handleNewServerConnection();
            else if(fd==m_sockAboutClient)  //如果是客户端监听套接字
                handleNewClientConnection();  
            else if(events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR))    //如果事件类型是错误、远端关闭挂起、关闭连接等
                delServer(fd); 
            else if(events&EPOLLIN)           //如果是可读事件,即服务器发送更新信息到达
                recvServerPack(fd);     
        }
    }
}

// 初始化均衡器Tcp
bool Equalizer::tcpInit(std::string &ip, const int &port_server, const int &port_client)
{
   //创建套接字
    int ret=0;

    if(port_server>65535||port_server<1024||port_client>65535||port_client<1024){
        return false;
    }

    m_sockAboutServer=socket(AF_INET,SOCK_STREAM,0);        //处理与服务器交互sock
    m_sockAboutClient=socket(AF_INET,SOCK_STREAM,0);        //处理客户端任务套接字

    //设置套接字选项
    int on=1;
    struct linger optLinger = { 0 };
    optLinger.l_linger=1;
    optLinger.l_onoff=1;
    ret=setsockopt(m_sockAboutServer,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));  //设置接受端口重用
    ret=setsockopt(m_sockAboutClient,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));  //设置接受端口重用
    ret = setsockopt(m_sockAboutServer, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger)); //设置套接字优雅关闭
    ret = setsockopt(m_sockAboutClient, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger)); //设置套接字优雅关闭

    //绑定套接字
    struct sockaddr_in sockinfo;
    bzero(&sockinfo,sizeof(sockinfo));
    sockinfo.sin_addr.s_addr=inet_addr(ip.c_str());
    sockinfo.sin_family=AF_INET;
    sockinfo.sin_port=htons(port_server);

    ret=bind(m_sockAboutServer,(sockaddr *)&sockinfo,sizeof(sockinfo));

    struct sockaddr_in clintinfo=sockinfo;
    clintinfo.sin_port=htons(port_client);
    ret=bind(m_sockAboutClient,(sockaddr *)&clintinfo,sizeof(clintinfo));

    //开始监听
    ret=listen(m_sockAboutServer,10);
    ret=listen(m_sockAboutClient,10);
    if(ret==-1)
        return false;
    SetFdNonblock(m_sockAboutServer);                              //设置套接字非阻塞
    SetFdNonblock(m_sockAboutClient);                              //设置套接字非阻塞
    m_epoll.AddFd(m_sockAboutServer,listenEvent_);        //将套接字添加到epoll监控,监控默认事件和可读事件
    m_epoll.AddFd(m_sockAboutClient,listenEvent_);        //将套接字添加到epoll监控,监控默认事件和可读事件
    return true;    
}

//处理新服务器连接
void Equalizer::handleNewServerConnection()
{
    struct sockaddr_in addr;
    socklen_t len=sizeof(addr);
    do{
        int fd=accept(m_sockAboutServer,(struct sockaddr *)&addr,&len);
        if(fd<=0)return;    //已经无新连接
        recvServerInfo(fd);
    }while(true);
}


//处理新客户端请求
void Equalizer::handleNewClientConnection()
{
    struct sockaddr_in addr;
    socklen_t len=sizeof(addr);
    do{
        int fd=accept(m_sockAboutClient,(struct sockaddr *)&addr,&len);
        if(fd<=0)return;    //已经无新连接
        sendServerInfoToClient(fd);
    }while(true);
}

// 设置sock为非阻塞
int Equalizer::SetFdNonblock(int sock)
{
    assert(sock>0);
    return fcntl(sock, F_SETFL, fcntl(sock, F_GETFD, 0) | O_NONBLOCK);
}


//显示全部服务器信息
void Equalizer::displayServerInfo()
{
    std::vector<ServerNode> ptr=m_serverHeap.GetVet();
    system("clear");
    std::cout<<"服务器名            IP地址          短任务端口          长任务端口          当前连接数"<<std::endl;
    for(int i=0;i<ptr.size();i++)
    {
        std::cout<<ptr.at(i).servername<<"          ";
        std::cout<<ptr.at(i).ip<<"          ";
        std::cout<<ptr.at(i).shotPort<<"                ";
        std::cout<<ptr.at(i).longPort<<"                ";
        std::cout<<ptr.at(i).curConCount<<"         "<<endl;
    }
}

//接受服务器初始连接信息，并添加到堆中
void Equalizer::recvServerInfo(int sock)
{
    try
    {
        ServerInfoPack pack;
        char key[60] = {0};
        bool res = false;

        // 依次读取服务器信息，并检查 read 的返回值
        if (read(sock, pack.servername, sizeof(pack.servername)) <= 0) {
            std::cerr << "读取服务器名称失败" << std::endl;
            close(sock);
            return;
        }

        if (read(sock, pack.ip, sizeof(pack.ip)) <= 0) {
            std::cerr << "读取服务器 IP 失败" << std::endl;
            close(sock);
            return;
        }

        if (read(sock, &pack.sport, sizeof(pack.sport)) <= 0) {
            std::cerr << "读取服务器短连接端口失败" << std::endl;
            close(sock);
            return;
        }

        if (read(sock, &pack.lport, sizeof(pack.lport)) <= 0) {
            std::cerr << "读取服务器长连接端口失败" << std::endl;
            close(sock);
            return;
        }

        if (read(sock, &pack.curConCount, sizeof(pack.curConCount)) <= 0) {
            std::cerr << "读取当前连接数失败" << std::endl;
            close(sock);
            return;
        }

        if (read(sock, key, sizeof(key)) <= 0) {
            std::cerr << "读取密钥失败" << std::endl;
            close(sock);
            return;
        }

        // 检查密钥是否正确
        if (std::string(key) != ConKey) {
            std::cerr << "连接密钥不正确，关闭连接" << std::endl;
            send(sock, &res, sizeof(res), 0);  // 发送 false 表示密钥错误
            close(sock);
            return;
        }

        // 将服务器信息添加到堆中
        addServer(sock, pack.curConCount, pack.ip, pack.sport, pack.lport, pack.servername);

        // 设置非阻塞模式
        SetFdNonblock(sock);

        // 将 sock 添加到 epoll 监控的文件描述符中
        m_epoll.AddFd(sock, listenEvent_);

        // 发送 true 表示成功
        res = true;
        if (send(sock, &res, sizeof(res), 0) < 0) {
            std::cerr << "发送成功响应时出错: " << strerror(errno) << std::endl;
        }

        std::cout << "服务器: " << pack.servername << " 连接成功" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "接收服务器信息时发生异常: " << e.what() << std::endl;
        close(sock);  // 确保在异常情况下关闭 socket
    }
}

//接受服务器信息体，并更新服务器信息
void Equalizer::recvServerPack(int sock)
{
    assert(sock>0);
    ServerState state;

    recv(sock,&state,sizeof(state),0);

    //std::cout<<"当前状态为:"<<state.code<<std::endl;
    //std::cout<<"当前连接数:"<<state.curConCount<<std::endl;

    if(state.code==0){
        updateServerState(sock,state.curConCount);  //如果状态为0，非关闭更新状态
    }
    else if(state.code==1)
        delServer(sock);                            //关闭状态，从堆中删除该服务器
}

// 更新服务器的当前连接数
void Equalizer::updateServerState(int sock,size_t newConCount)
{
    assert(sock>0);
    //std::cout<<"服务器："<<sock<<" 更新"<<std::endl;
    m_serverHeap.adjust(sock,newConCount);
}

//添加服务器
void Equalizer::addServer(int sock,const size_t &count,const std::string &addr,const uint16_t &sPort,const uint16_t &lPort,const string &servername)
{
    assert(sock>0);
    m_serverHeap.add(sock,addr,sPort,lPort,count,servername);
}

//删除服务器
void Equalizer::delServer(int sock)
{
    assert(sock>0);
    std::cout<<"服务器"<<sock<<" 删除"<<std::endl;
    m_serverHeap.pop(sock);
    m_epoll.DelFd(sock);
    close(sock);
}

//发送服务器信息给客户端
void Equalizer::sendServerInfoToClient(int sock)
{
    try
    {
        if (sock <= 0) {
            std::cerr << "无效的客户端套接字，无法发送服务器信息" << std::endl;
            return;
        }   

        ServerNode curServer;
        m_serverHeap.GetMinServerInfo(curServer);
        
        ServerInfoPack pack(curServer.ip, curServer.shotPort, curServer.longPort,curServer.servername);

        ssize_t bytes_sent = send(sock, pack.ip, sizeof(pack.ip), 0); // 发送20字节
        if (bytes_sent < 0) {
            std::cerr << "发送服务器IP信息时出错: " << strerror(errno) << std::endl;
            close(sock);
            return;
        }

        bytes_sent = send(sock, &pack.sport, sizeof(pack.sport), 0); // 发送4字节
        if (bytes_sent < 0) {
            std::cerr << "发送服务器sport信息时出错: " << strerror(errno) << std::endl;
            close(sock);
            return;
        }

        bytes_sent = send(sock, &pack.lport, sizeof(pack.lport), 0); // 发送4字节
        if (bytes_sent < 0) {
            std::cerr << "发送服务器lport信息时出错: " << strerror(errno) << std::endl;
            close(sock);
            return;
        }

        close(sock); // 发送成功后关闭连接
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }   
}
