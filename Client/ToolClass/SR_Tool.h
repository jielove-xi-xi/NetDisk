#pragma once

#include <QObject>
#include<asio.hpp>
#include<asio/ssl.hpp>
#include<atomic>
#include"../function.h"
using namespace asio;

typedef std::shared_ptr < ip::tcp::socket>socket_ptr;					//定义为普通套接字共享指针共享指针
typedef std::shared_ptr < ssl::stream<ip::tcp::socket>>sslsock_ptr;		//安全套接字共享指针


//用于简易处理与服务端交互的工具，网络收发使用Asio库实现.可使用同步发送和异步发送
class SR_Tool  : public QObject,public std::enable_shared_from_this<SR_Tool>
{
	Q_OBJECT

public:
	SR_Tool(QObject* parent,const std::string &enpaddr,const int &enpPort);		//传递对端地址和端口进行构造
	~SR_Tool();

//同步处理接口，传递一个asio::error_code,同步操作后将结果存放在ec
public:
	void connect(asio::error_code& ec);			//同步连接
	void send(char* buf, size_t len,asio::error_code& ec);			//同步发送
	void recv(char* buf, size_t len, asio::error_code& ec);			//同步接收
	ssl::stream<ip::tcp::socket>* getSsl();							//返回ssl指针
	ip::tcp::endpoint GetEndPoint();								//返回对端对象

	bool sendPdu(const PDU& pud);									//将一个pdu发送到服务器
	bool recvRespon(ResponsePack& res);								//同步接受一个回复体
	bool recvFileInfo(FILEINFO& fileinfo);							//接受一条文件信息

//异步处理接口，异步事件完成会触发相关信号。连接信号，接收异步结果。也可以传递回调函数，在异步事件就绪时候，调用回调函数。调用异步任务后，需要SR_run()启动异步循环
public:
	void async_connect();							//异步连接服务器
	void async_send(char* buf, size_t len);			//异步发送数据
	void async_recv(char* buf, size_t len);			//异步接收len字节数据到buf

	void async_connect(std::function<void()>fun);	//传递函数对象，异步连接
	void async_send(char* buf, size_t len, std::function<void()>fun);	//传递函数对象，异步发送
	void async_recv(char* buf, size_t len, std::function<void()>fun);	//传递函数对象，异步接收

	void SR_run();									//开始异步任务
	void SR_stop();									//结束异步任务
//异步信号
signals:	
	void connected();	//连接成功
	void sendOK();		//发生成功
	void recvOK();		//接收成功
	void error(QString message);	//错误信号
private:
	//异步回调函数
	void connect_handler(const error_code& ec);		//异步连接成功后回调函数
	void send_handler(const error_code& ec, std::size_t bytes_transferred);		//异步发送成功回调函数
	void recv_handler(const error_code& ec, std::size_t bytes_transferred);		//异步接收成功回调函数
private:
	std::shared_ptr <io_context> m_Context;					//IO上下文
	std::shared_ptr<ssl::context>	m_sslContext;			//ssl安全连接上下文
	sslsock_ptr m_sslsock;									//sock安全套接字
	ip::tcp::endpoint m_ep;									//保存对端地址和端口信息，即服务器地址和端口
	std::atomic<bool>m_running=true;						//用于控制异步任务停止
};
