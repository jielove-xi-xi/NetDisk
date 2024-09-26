#pragma once

#include <QDialog>
#include<QMessageBox>
#include "ui_Login.h"
#include"../ToolClass/SR_Tool.h"
#include"../function.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LoginClass; };
QT_END_NAMESPACE

class Login : public QDialog
{
	Q_OBJECT

public:
	Login(std::shared_ptr<SR_Tool>srTool,UserInfo *info,QWidget* parent = nullptr);
	~Login();

private:
	void Tologin();		//执行登陆操作
	void sign();		//执行注册操作
	void connection();	//执行连接操作
private slots:
	void on_btnLogin_clicked();		//登陆按钮槽函数
	void on_btnSign_clicked();		//注册按钮槽函数
	void do_Connected();	//连接成功会执行
	void do_Send();			//发送成功后执行
	void do_recved();		//接收成功后执行
	void do_error(QString message);		//出错就执行
	void do_ArrToInfo();				//将接收到的字节数据转换到用户信息结构体
private:
	Ui::LoginClass *ui;
	std::shared_ptr<SR_Tool>m_srTool;		//用来进行通信
	bool m_loginOrSign=true;				//区别是注册操作还是登陆操作
	bool m_connected = false;				//是否已经连接到服务器

	char m_buf[1024] = { 0 };				//保存接收到数据的缓冲区
	char m_recvbuf[1024] = { 0 };		
	char m_infobuf[1024] = { 0 };	//保存接收到的用户信息缓冲区


	QString m_user;					//用户名
	QString m_pwd;					//密码
	UserInfo* m_info = nullptr;		//保存服务器发送回来的用户信息
};
