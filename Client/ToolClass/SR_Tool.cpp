#include"SR_Tool.h"

SR_Tool::SR_Tool(QObject* parent, const std::string& enpaddr, const int& enpPort)
	: QObject(parent), m_ep(asio::ip::address::from_string(enpaddr), enpPort)
{
	m_Context = std::make_shared<asio::io_context>();
	m_sslContext = std::make_shared<ssl::context>(asio::ssl::context::tlsv13_client);
	m_sslContext->set_default_verify_paths();
	m_sslsock = std::make_shared< ssl::stream<ip::tcp::socket>>(*m_Context.get(), *m_sslContext.get());
}

SR_Tool::~SR_Tool()
{
	m_running = false;	//结束异步任务
}

//同步连接，完成将连接状态保存在ec
void SR_Tool::connect(asio::error_code &ec)
{
	m_sslsock->lowest_layer().connect(m_ep,ec);		//底层套接字连接
	m_sslsock->handshake(ssl::stream<ip::tcp::socket>::client,ec);	//ssl握手
}

//同步发送，完成将连接状态保存在ec
void SR_Tool::send(char* buf, size_t len,asio::error_code& ec)
{
	m_sslsock->write_some(asio::buffer(buf, len), ec);
}

//同步接收，完成将连接状态保存在ec
void SR_Tool::recv(char* buf, size_t len, asio::error_code& ec)
{
	asio::read(*m_sslsock.get(), asio::buffer(buf, len), ec);
}

//返回ssl指针
ssl::stream<ip::tcp::socket>* SR_Tool::getSsl()
{
	return m_sslsock.get();
}

//返回对端对象
ip::tcp::endpoint SR_Tool::GetEndPoint()
{
	return m_ep;
}

//同步发送一个PDU
bool SR_Tool::sendPdu(const PDU& pdu)
{
	char buf[PDULEN] = { 0 };
	asio::error_code ec;
	if (PduToArr(pdu, buf))
	{
		if (pdu.len == 0)
			send(buf, PDULEN - PDUReserlen, ec);	//不发生预留空间
		else
			send(buf, PDULEN - PDUReserlen + pdu.len, ec);	//发送预留空间
	}
	else
	{
		emit error("pdu to Arr Error");
		return false;
	}

	if (!ec)
		return true;
	else
		emit error(QString::fromStdString("send error " + ec.message()));
	return false;
}

//同步接受一个回复体
bool SR_Tool::recvRespon(ResponsePack& res)
{
	asio::error_code ec;
	recv((char*)&res.code, sizeof(res.code), ec);	//读取code码
	if (ec)return false;
	recv((char*)&res.len, sizeof(res.len), ec);		//读取缓冲区长度
	if (ec)return false;
	if (res.len > 0)
		recv(res.buf, res.len, ec);
	if (!ec)
		return true;
	else
		emit error(QString::fromStdString("recv error " + ec.message()));
	return false;
}

//接受一条文件信息
bool SR_Tool::recvFileInfo(FILEINFO& fileinfo)
{
	asio::error_code ec;

	recv((char*)&fileinfo.FileId, sizeof(fileinfo.FileId), ec);
	if (ec) return false;

	recv(fileinfo.Filename, sizeof(fileinfo.Filename), ec);
	if (ec) return false;

	recv((char*)&fileinfo.DirGrade, sizeof(fileinfo.DirGrade), ec);
	if (ec) return false;

	recv(fileinfo.FileType, sizeof(fileinfo.FileType), ec);
	if (ec) return false;

	recv((char*)&fileinfo.FileSize, sizeof(fileinfo.FileSize), ec);
	if (ec) return false;

	recv((char*)&fileinfo.ParentDir, sizeof(fileinfo.ParentDir), ec);
	if (ec) return false;

	recv(fileinfo.FileDate, sizeof(fileinfo.FileDate), ec);
	if (ec) return false;

	return true; // 没有发生错误
}

//异步连接对端，成功将发送connected()信号
void SR_Tool::async_connect()
{
	//底层套接字异步，成功后，调用匿名函数进行异步ssl握手
	auto self = shared_from_this();	//获取本对象的共享指针
	m_sslsock->lowest_layer().async_connect(m_ep, [self](const error_code& ec)
		{
			if (!ec) {
				self->m_sslsock->async_handshake(ssl::stream<ip::tcp::socket>::client,
					std::bind(&SR_Tool::connect_handler, self, std::placeholders::_1));
			}
			else 
				emit self->error(QString::fromLocal8Bit(ec.message()));
		});
}

//传递仿函数，异步连接，连接成功调用仿函数
void SR_Tool::async_connect(std::function<void()> fun)
{
	auto self = shared_from_this();		//获取共享对象

	//异步连接，连接成功调用匿名函数，在调用仿函数
	m_sslsock->lowest_layer().async_connect(m_ep, [self, fun](const error_code& ec)
		{
			if (!ec)
				self->m_sslsock->async_handshake(asio::ssl::stream<ip::tcp::socket>::client, [self,fun](const error_code& ec)
					{
						if (!ec)
							fun();
						else 
							emit self->error(QString::fromLocal8Bit(ec.message()));
					});
			else 
				emit self->error(QString::fromLocal8Bit(ec.message()));
		});
}

//将缓冲区buf的len字节数据异步发送，成功后调用函数对象执行
void SR_Tool::async_send(char* buf, size_t len, std::function<void()> fun)
{
	auto self = shared_from_this();
	asio::async_write(*m_sslsock.get(), asio::buffer(buf, len), [self, fun](const error_code& ec, std::size_t bytes_transferred) {
		if (!ec)
			fun();
		else
			emit self->error(QString::fromLocal8Bit(ec.message()));
		});
}

//将len字节数据异步存进缓冲区buf，成功后调用函数对象执行
void SR_Tool::async_recv(char* buf, size_t len, std::function<void()> fun)
{
	auto self = shared_from_this();
	asio::async_read(*m_sslsock.get(), asio::buffer(buf, len), [self, fun](const error_code& ec, std::size_t bytes_transferred) {
		if (!ec)
			fun();
		else
			emit self->error(QString::fromLocal8Bit(ec.message()));
		});
}


//将缓存区buf的len字节数据，异步发生到对端，成功将发送sendOK()信号
void SR_Tool::async_send(char* buf, size_t len)
{
	auto self = shared_from_this();
	asio::async_write(*m_sslsock.get(), asio::buffer(buf, len),
		std::bind(&SR_Tool::send_handler, self, std::placeholders::_1, std::placeholders::_2)); //异步发送数据
}



//异步接收len字节数据到缓冲区buf中，成功发送recvOK信号
void SR_Tool::async_recv(char* buf, size_t len)
{
	auto self = shared_from_this();
	asio::async_read(*m_sslsock.get(), asio::buffer(buf, len),
		std::bind(&SR_Tool::recv_handler, self, std::placeholders::_1, std::placeholders::_2));	//异步接收
}

//开始异步任务
void SR_Tool::SR_run()
{
	m_running = true;
	auto self = shared_from_this();
	std::thread([self] {
		try {
			while (self->m_running) {
				if (self->m_Context->run() == 0)
					std::this_thread::sleep_for(std::chrono::milliseconds(10));	//无任务线程休眠，避免cpu耗用太大

			}
		}
		catch (const std::exception& e) {
			emit self->error(QString::fromLocal8Bit(e.what()));
		}
		}).detach();	//放在独立线程运行，避免UI阻塞
}

void SR_Tool::SR_stop()
{
	m_running = false;
}

//连接成功回调函数
void SR_Tool::connect_handler(const error_code& ec)
{
	if (!ec)
		emit connected();		//发生连接成功信号
	else
		emit error(QString::fromLocal8Bit(ec.message()));
}

//发送成功回调函数
void SR_Tool::send_handler(const error_code& ec, std::size_t bytes_transferred)
{
	if (!ec) {
		emit sendOK();	//发送成功信号
	}
	else
		emit error(QString::fromLocal8Bit(ec.message()));
}

//接收成功回调函数
void SR_Tool::recv_handler(const error_code& ec, std::size_t bytes_transferred)
{
	if (!ec)
		emit recvOK();	//发送接收成功信号
	else
		emit error(QString::fromLocal8Bit(ec.message()));
}
