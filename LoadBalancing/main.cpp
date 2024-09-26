#include"equalizer.h"
#include <csignal>
#include <cerrno>
#include <cstring>

int main()
{
    signal(SIGPIPE, SIG_IGN);       //必须忽略这个信号，否则均衡器非常容易崩溃
    // std::string ip;
    // uint16_t server_port;
    // uint16_t client_port;
    // string key;
    // std::cout<<"请输入均衡器地址,服务器交互端口和客户端交互端口,以及连接密码（不超过60字节，非中文）"<<std::endl;
    // cin>>ip>>server_port>>client_port>>key;
    // if(key.size()>60)
    // {
    //     std::cout<<"密码太长"<<std::endl;
    //     return 0;
    // }
    Equalizer mainserver("127.0.0.1",9090,9091,"abc9988220");
    mainserver.start();
    return 0;
}