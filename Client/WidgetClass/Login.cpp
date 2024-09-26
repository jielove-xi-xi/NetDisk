#include "Login.h"
Login::Login(std::shared_ptr<SR_Tool>srTool, UserInfo* info, QWidget* parent)
	: QDialog(parent),m_srTool(srTool),m_info(info)
	, ui(new Ui::LoginClass())
{
	ui->setupUi(this);
	connect(m_srTool.get(), &SR_Tool::sendOK, this, &Login::do_Send, Qt::QueuedConnection);
	connect(m_srTool.get(), &SR_Tool::connected, this, &Login::do_Connected,Qt::QueuedConnection);
	
	connect(m_srTool.get(), &SR_Tool::recvOK, this, &Login::do_recved, Qt::QueuedConnection);
	connect(m_srTool.get(), &SR_Tool::error, this, &Login::do_error, Qt::QueuedConnection);
	
	this->setWindowTitle("登陆");
}

Login::~Login()
{
	m_srTool->SR_stop();	//停止异步任务
	disconnect(m_srTool.get(), &SR_Tool::connected, this, &Login::do_Connected);
	disconnect(m_srTool.get(), &SR_Tool::sendOK, this, &Login::do_Send);
	disconnect(m_srTool.get(), &SR_Tool::recvOK, this, &Login::do_recved);
	disconnect(m_srTool.get(), &SR_Tool::error, this, &Login::do_error);
	delete ui;
}


//执行登陆操作
void Login::Tologin()
{
	PDU pdu;
	pdu.PDUcode = Code::LogIn;

	memcpy(pdu.user, m_user.toUtf8().data(), m_user.size());
	memcpy(pdu.pwd, m_pwd.toUtf8().data(), m_pwd.size());
	memset(m_buf, 0, PDULEN);
	if (PduToArr(pdu, m_buf))
	{
		if (pdu.len == 0)
			m_srTool->async_send(m_buf, PDULEN-PDUReserlen);	//不用发送预留空间
		else
			m_srTool->async_send(m_buf, PDULEN - PDUReserlen + pdu.len);	//把预留空间的字节树也发过去
	}	
	else
		QMessageBox::information(this, "登陆错误", "发送数据出错");
	
}

//执行注册操作
void Login::sign()
{
	PDU pdu;
	pdu.PDUcode = Code::SignIn;

	memcpy(pdu.user, m_user.toUtf8().data(), m_user.size());
	memcpy(pdu.pwd, m_pwd.toUtf8().data(), m_pwd.size());
	memset(m_buf, 0, PDULEN);
	if (PduToArr(pdu, m_buf))
	{
		if (pdu.len == 0)
			m_srTool->async_send(m_buf, PDULEN - PDUReserlen);	//不用发送预留空间
		else
			m_srTool->async_send(m_buf, PDULEN - PDUReserlen + pdu.len);	//把预留空间的字节数也发过去
	}
	else
		QMessageBox::information(this, "登陆错误", "发送数据出错");
}

//执行连接操作
void Login::connection()
{	
	if (m_connected == false)	//如果还没连接
	{
		auto fun = std::bind(&Login::do_Connected, this);
		m_srTool->async_connect(fun);
	}
	else
		do_Connected();	//直接执行连接成功槽函数
	m_srTool->SR_run();	//开始异步任务
}

//发送成功槽函数
void Login::do_Send()
{
	memset(m_recvbuf, 0, Responlen);
	m_srTool->async_recv(m_recvbuf, Responlen-ResBuflen);		//异步接收服务端回复的包体
}

//接收成功槽函数
void Login::do_recved()
{
	ResponsePack response;
	if (ArrToRespon(m_recvbuf, response))
	{
		if (response.code == Code::CodeOK) {
			asio::error_code ec;
			m_srTool->recv(m_infobuf,UserInfoLen,ec);
			if (!ec)
			{
				do_ArrToInfo();
				QDialog::accept();
			}
		}
		else
		{
			if(m_loginOrSign)
				QMessageBox::warning(this, "登陆失败", "用户名或者密码不正确");
			else
				QMessageBox::warning(this, "注册失败", "用户名已经存在");
		}
	}
	else
	{
		QMessageBox::warning(this, "警告", "接收数据错误");

	}
	m_srTool->SR_stop();	//停止异步操作
}

//出现错误槽函数
void Login::do_error(QString message)
{
	QMessageBox::warning(this, "错误", message);
}

void Login::do_ArrToInfo()
{
	size_t offset = 0;
	memcpy(m_info->username, m_infobuf+offset, sizeof(m_info->username));
	offset += sizeof(m_info->username);
	memcpy(m_info->pwd, m_infobuf + offset, sizeof(m_info->pwd));
	offset += sizeof(m_info->pwd);
	memcpy(m_info->cipher, m_infobuf + offset, sizeof(m_info->cipher));
	offset += sizeof(m_info->cipher);
	memcpy(m_info->isvip, m_infobuf + offset, sizeof(m_info->isvip));
	offset += sizeof(m_info->isvip);
	memcpy(m_info->capacitySum, m_infobuf + offset, sizeof(m_info->capacitySum));
	offset += sizeof(m_info->capacitySum);
	memcpy(m_info->usedCapacity, m_infobuf + offset, sizeof(m_info->usedCapacity));
	offset += sizeof(m_info->usedCapacity);
	memcpy(m_info->salt, m_infobuf + offset, sizeof(m_info->salt));
	offset += sizeof(m_info->salt);
	memcpy(m_info->vipdate, m_infobuf + offset, sizeof(m_info->vipdate));
	offset += sizeof(m_info->vipdate);
}

//登陆按钮
void Login::on_btnLogin_clicked()
{
	m_loginOrSign = true;
	m_user = ui->Edituser->text();
	m_pwd = ui->EditPwd->text();
	connection();
}

//登陆按钮槽函数
void Login::on_btnSign_clicked()
{
	m_loginOrSign = false;	//执行注册操作
	m_user = ui->Edituser->text();
	m_pwd = ui->EditPwd->text();
	connection();
}

//连接成功槽函数
void Login::do_Connected()
{
	m_connected = true;
	if (m_loginOrSign)	//执行登陆操作
		Tologin();
	else
		sign();			//执行注册操作
}
