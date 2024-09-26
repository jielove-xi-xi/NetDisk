#include "ShortTaskManage.h"


ShortTaskManage::ShortTaskManage(std::shared_ptr<SR_Tool>srTool, QObject* parent):m_srTool(srTool), QObject(parent)
{
}

ShortTaskManage::~ShortTaskManage()
{

}


//获取文件列表
void ShortTaskManage::GetFileList()
{
	//创建pdu
	bool status;
	PDU pdu;
	pdu.PDUcode = Code::CD;	//指定为CD任务
	
	status=m_srTool->sendPdu(pdu);		//将pdu发送到服务器
	if (status == false)
	{
		emit error("GetFileList():send pdu error");
		return;
	}

	ResponsePack response;
	status=m_srTool->recvRespon(response);	//接受回复体
	if (status == false)
	{
		emit error("GetFileList():recv response error");
		return;
	}

	//如果服务端回复错误信息
	if (response.code == Code::LogIn) {
		emit error("Clinet must need login");
		return;
	}

	std::size_t filecount;
	std::shared_ptr<std::vector<FILEINFO>>filevet;

	if (response.code == Code::CodeNO)	//没有文件
		filecount = 0;
	else
		memcpy(&filecount, response.buf, sizeof(std::size_t));	//将文件数目拷贝到filecount
	
	 filevet=std::make_shared< std::vector<FILEINFO>>(filecount);	//创建一个filecount大小容器

	//开始逐一接受文件信息
	for (int i = 0; i < filecount; i++)
	{
		FILEINFO info;
		status=m_srTool->recvFileInfo(info);
		if(status)
			filevet->at(i) = info;
		else
		{
			emit error("recv file info error");
			return;
		}
	}

	if(filecount>0)
		std::sort(filevet->begin(), filevet->end(), [](FILEINFO& a, FILEINFO& b) {return a.DirGrade < b.DirGrade; });	//将其按照文件等级从小到达排序

	emit GetFileListOK(filevet);	//将保存文件信息的容器，连同成功信号一起发送出去
}

//创建新文件夹
void ShortTaskManage::MakeDir(quint64 parentid, QString newdirname)
{
	PDU pdu;
	ResponsePack res;
	pdu.PDUcode = Code::MAKEDIR;

	// 确保文件名不会超过 pdu.filename 的大小
	QByteArray dirnameBytes = newdirname.toUtf8();
	if (dirnameBytes.size() >= sizeof(pdu.filename)) {
		emit error("Directory name too long");
		return;
	}
	std::uint64_t senid = parentid;
	pdu.len = sizeof(std::uint64_t);
	memcpy(pdu.filename, dirnameBytes.data(), dirnameBytes.size());  // 复制文件夹名到 pdu.filename
	memcpy(pdu.reserve, &senid, sizeof(std::uint64_t));                // 复制父文件夹ID到 pdu.reserve

	// 发送 PDU 请求
	m_srTool->sendPdu(pdu);

	// 接收响应
	m_srTool->recvRespon(res);

	// 处理服务器的响应
	std::uint64_t newid = 0;
	if (res.len >= sizeof(newid)) {
		memcpy(&newid, &res.buf, sizeof(newid));  // 从响应中提取文件夹 ID
	}
	else {
		emit error("Invalid response from server");
		return;
	}

	// 判断响应码
	if (res.code == Code::CodeOK) {
		emit sig_createDirOK(newid, parentid, newdirname);  // 创建成功
	}
	else {
		emit error("Create directory error");  // 处理创建失败的情况
	}
}

//删除文件
void ShortTaskManage::del_File(quint64 fileid, QString filename)
{
	PDU pdu;
	ResponsePack res;
	pdu.PDUcode = Code::DELETEFILE;

	pdu.len = sizeof(std::uint64_t);
	memcpy(pdu.filename, filename.toStdString().c_str(), filename.size());			// 复制文件夹名到 pdu.filename
	memcpy(pdu.pwd, &fileid, sizeof(fileid));                 // 复制父文件夹ID到 pdu.reserve

	// 发送 PDU 请求
	m_srTool->sendPdu(pdu);

	// 接收响应
	m_srTool->recvRespon(res);

	// 判断响应码
	if (res.code == Code::CodeOK) {
		emit sig_delOK();
	}
	else {
		emit error("delete directory error");  // 处理创建失败的情况
	}
}
