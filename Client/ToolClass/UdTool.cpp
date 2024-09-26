#include "UdTool.h"

UdTool::UdTool(const QString& IP, const int& Port,QObject *parent): QObject(parent), m_srTool(nullptr,IP.toStdString(),Port)
{

}

UdTool::~UdTool()
{
	if (m_file.isOpen())
		m_file.close();
	m_srTool.getSsl()->lowest_layer().close();	//如果非正常退出，可以直接断开底层套接字，不影响ssl
}

//设置要上传或者下载的文件名
void UdTool::SetTranPdu(const TranPdu &tranpdu)
{
	m_TranPdu = tranpdu;
	m_filename = QString(m_TranPdu.filename);	//设置文件全名，即包括磁盘路径名。
	m_file.setFileName(m_filename);
	if (m_TranPdu.TranPduCode == Code::Puts) {
		if (!m_file.open(QIODevice::ReadOnly))	//提前打开，如果打开失败
		{
			emit erroroccur("Error In function:UdTool::SetTranPdu(const TranPdu &tranpdu),open file error");
			return;
		}
		m_TranPdu.filesize = m_file.size();		//初始化文件大小
	}
	
	QFileInfo fileinfo(m_filename);
	std::string filesuffix = fileinfo.fileName().toStdString();		//只向服务端发送后缀不带路径的名字
	memcpy(m_TranPdu.filename, filesuffix.c_str(), filesuffix.size()+1);
}

//设置传输控制对象
void UdTool::SetControlAtomic(std::shared_ptr <std::atomic<int>>control,
	std::shared_ptr<std::condition_variable>cv, std::shared_ptr<std::mutex>mutex)
{
	m_control = control;
	m_cv = cv;
	m_mutex = mutex;
}

//计算文件哈希值
bool UdTool::calculateSHA256()
{
	SHA256_CTX sha256Context;	//sha256上下文
	SHA256_Init(&sha256Context);

	QByteArray fileData;
	const int bufferSize = 4096;
	char buffer[bufferSize];
	int bytesRead = 0;

	if (m_file.isOpen())
	{
		while ((bytesRead = m_file.read(buffer, bufferSize)) > 0) 
			SHA256_Update(&sha256Context, buffer, bytesRead);
	}
	else
		return false;

	unsigned char result[SHA256_DIGEST_LENGTH];
	SHA256_Final(result, &sha256Context);

	// 将 SHA-256 结果转换为 QByteArray
	QByteArray sha256Result(reinterpret_cast<char*>(result), SHA256_DIGEST_LENGTH);
	
	if (sha256Result.isEmpty())
	{
		return false;
	}
	else
	{
		QByteArray arryhex = sha256Result.toHex();
		if (sizeof(m_TranPdu.FileMd5) < arryhex.size() + 1)
			return false;
		memcpy(m_TranPdu.FileMd5, arryhex.data(), arryhex.size());
		m_TranPdu.FileMd5[arryhex.size()] = '\0';
		return true;
	}
}

//发送PDU到服务端
bool UdTool::sendTranPdu()
{
	TranPdu::TranPduToArr(m_TranPdu, buf);
	m_srTool.send(buf, TranPduLen, m_ec);
	if (!m_ec)
		return true;
	else
	{
		return false;
	}
}


//接收服务器回复报文
bool UdTool::recvRespon()
{
	m_srTool.recvRespon(m_res);
	if (!m_ec) {
		if (m_res.code == Code::CodeOK || m_res.code == Code::PutsContinueNO||m_res.code==::Code::PutQUICKOK)
			return true;
		else 
			return false;
	}
	else
		return false;
}

//开始发送文件
bool UdTool::sendFile()
{
	m_file.seek(0);  // 从零开始传输
	qint64 total = m_TranPdu.filesize;  // 发送总字节
	QByteArray buf;
	qint64 bytes = 1024;
	qint64 sended = 0;					// 已经发送字节

	// 进度更新的变量
	double lastProgress = 0;  // 上次发送进度的比例
	const double progressStep = 0.003;  // 每 1% 发送一次，避免频繁发送，影响线程工作


	//如果是普通上传任务
	if (m_TranPdu.TranPduCode == Code::Puts&&m_res.code!=Code::PutQUICKOK)	//不支持快传
	{
		//发送主循环
		while (sended < total)
		{
			if (m_control->load() == 1)		//传输控制，为1则暂停等待
			{
				std::unique_lock<std::mutex>lock(*m_mutex.get());
				m_cv->wait(lock, [this] {return m_control->load() != 1; });
				continue;
			}
			else if (m_control->load() == 2) //为2则结束传输
			{
				return false;
			}

			bytes = (total - sended) > 1024 ? 1024 : (total - sended);
			buf = m_file.read(bytes);
			if (!buf.isEmpty())  // 如果不为空
			{
				m_srTool.send(buf.data(), buf.size(), m_ec);
				if (!m_ec) 
				{
					sended += buf.size();

					// 计算当前的进度比例
					double currentProgress = static_cast<double>(sended) / total;
					if (currentProgress - lastProgress >= progressStep) 
					{
						emit sendProgress(sended, total);
						lastProgress = currentProgress;
					}
					buf.clear();
				}
				else
					return false;
			}
		}
	}
	else if (m_TranPdu.TranPduCode == Code::PutsContinue)  // 客户端选择断点续传，暂不实现
	{
		// 断点续传逻辑可以在这里实现
	}
	ResponsePack pack;
	std::uint64_t fileid=0;
	m_srTool.recvRespon(pack);
	if (m_srTool.getSsl() && m_srTool.getSsl()->lowest_layer().is_open())
		m_srTool.getSsl()->shutdown();
	if (pack.code == Code::CodeOK) {
		if (pack.len != 0) 
			memcpy(&fileid, pack.buf, sizeof(std::uint64_t));		//接收服务器返回的文件ID
		emit sendProgress(total, total);  // 最后发送一次进度完成
		emit sendItemData(m_TranPdu, fileid);
		return true;
	}
	else
		return false;
}


//接收文件数据
bool UdTool::recvFile()
{
	if (!m_file.open(QIODevice::ReadWrite)) {  // 以读写方式打开文件
		emit erroroccur("Error opening file for writing");
		return false;
	}

	qint64 total = m_TranPdu.filesize;  // 总字节数
	qint64 received = 0;                // 已接收的字节数
	const qint64 chunkSize = 1024;      // 每次接收的块大小
	char buffer[chunkSize] = { 0 };
	double lastProgress = 0;
	const double progressStep = 0.003;  // 每 0.3% 更新一次进度

	while (received < total) {
		if (m_control->load() == 1) {  // 暂停下载
			std::unique_lock<std::mutex> lock(*m_mutex.get());
			m_cv->wait(lock, [this] { return m_control->load() != 1; });
			continue;
		}
		else if (m_control->load() == 2) {  // 终止下载
			return false;
		}

		// 计算要接收的字节数
		size_t bytesToReceive = (total - received) > chunkSize ? chunkSize : (total - received);
		m_srTool.recv(buf, bytesToReceive, m_ec);  //接受数据

		if (!m_ec) {
			m_file.write(buf, bytesToReceive);	//写到文件
			received += bytesToReceive;
			// 更新进度
			double currentProgress = static_cast<double>(received) / total;
			if (currentProgress - lastProgress >= progressStep) {
				emit sendProgress(received, total);  // 发送进度信号
				lastProgress = currentProgress;
			}
		}
		else {
			emit erroroccur("Error receiving file data: " + QString::fromStdString(m_ec.message()));
			return false;
		}
	}
	// 下载完成后，检查哈希值
	bool isOK;
	if (verifySHA256()) {
		isOK = true;
		m_srTool.send((char*)&isOK, sizeof(bool), m_ec);
		emit sendProgress(total, total);  // 下载完成，发送最终进度
		if (m_srTool.getSsl() && m_srTool.getSsl()->lowest_layer().is_open()) 
			m_srTool.getSsl()->shutdown();
		return isOK;
	}
	else {	
		//可以在这里实现客户选择重传
		emit erroroccur("File verification failed (SHA256 mismatch)");
		return false;
	}
	
}

//哈希检查
bool UdTool::verifySHA256()
{
	m_file.seek(0);	//重新将文件指针头指针

	QByteArray serverHash(m_res.buf, SHA256_DIGEST_LENGTH*2);

	SHA256_CTX sha256Context;
	SHA256_Init(&sha256Context);

	QByteArray fileData;
	const int bufferSize = 4096;
	char buffer[bufferSize];
	int bytesRead = 0;

	// 读取文件内容并更新 SHA256 计算
	while ((bytesRead = m_file.read(buffer, bufferSize)) > 0) {
		SHA256_Update(&sha256Context, buffer, bytesRead);
	}

	unsigned char result[SHA256_DIGEST_LENGTH];
	SHA256_Final(result, &sha256Context);
	m_file.close();

	// 将计算结果转换为 QByteArray
	QByteArray calculatedHash(reinterpret_cast<char*>(result), SHA256_DIGEST_LENGTH);

	// 将服务器返回的 SHA256 结果转换为 QByteArray 进行比较
	if (calculatedHash.toHex() == serverHash) {
		return true;
	}
	else {
		emit erroroccur("SHA256 hash mismatch. File may be corrupted.");
		return false;
	}
}

//移除文件
void UdTool::removeFileIfExists()
{
	if (m_file.exists()) {  // 如果文件存在
		if (!m_file.remove()) {  // 尝试删除文件
			emit erroroccur("Failed to remove file: " + m_filename);
		}
	}
}


//开始执行任务槽函数，连接QThread的started信号
void UdTool::doWorkUP()
{
	m_srTool.connect(m_ec);
	if (m_ec)
	{
		emit erroroccur("connect error"+QString::fromLocal8Bit(m_ec.message()));
		return;
	}
	if (!calculateSHA256())
	{
		emit erroroccur("create hash error");
		return;
	}
	if(!sendTranPdu())
	{
		emit erroroccur("send pdu error" + QString::fromLocal8Bit(m_ec.message()));
		return;
	}
	if (!recvRespon())
	{
		if(m_res.code==Code::CapacityNO)
			emit erroroccur("Capacity is not enough");
		else
			emit erroroccur("recv respon error");
		return;
	}
	if (!sendFile())
	{
		emit erroroccur("send data error" + QString::fromLocal8Bit(m_ec.message()));
		return;
	}
	emit workFinished();	//发送完成信号
}

//执行下载操作
void UdTool::doWorkDown() {
    m_srTool.connect(m_ec);  // 连接到服务器
    if (m_ec) {
        emit erroroccur("Connect error: " + QString::fromLocal8Bit(m_ec.message()));
        return;
    }

    if (!sendTranPdu()) {  // 发送下载请求
        emit erroroccur("Send PDU error: " + QString::fromLocal8Bit(m_ec.message()));
        return;
    }

    if (!recvRespon()) {  // 接收服务器响应
        if (m_res.code == Code::FileNOExist) {
            emit erroroccur("File does not exist");
        } else if (m_res.code == Code::GetsContinueNO) {
            emit erroroccur("Invalid file offset");
        } else {
            emit erroroccur("Receive response error: " + QString::fromLocal8Bit(m_ec.message()));
        }
        removeFileIfExists();  // 删除文件
        return;
    }

    if (!recvFile()) {  // 开始接收文件
        emit erroroccur("File receive end");
		removeFileIfExists();  // 删除文件
        return;
    }
    emit workFinished();  // 下载任务完成，发送完成信号
}
