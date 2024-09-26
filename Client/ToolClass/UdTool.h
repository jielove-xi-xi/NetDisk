#pragma once

#include <QObject>
#include<QFile>
#include<QFileDialog>
#include<QFileInfo>
#include<openssl/sha.h>
#include<atomic>
#include"../function.h"
#include"SR_Tool.h"

//负责上传和下载文件的类
class UdTool  : public QObject
{
	Q_OBJECT
public:
	UdTool(const QString& IP, const int& Port,QObject *parent=nullptr);
	~UdTool();
public:
	void SetTranPdu(const TranPdu& tranpdu);											//设置要上传和下载的文件名
	void SetControlAtomic(std::shared_ptr <std::atomic<int>>control,
		std::shared_ptr<std::condition_variable>cv, std::shared_ptr<std::mutex>mutex);	//设置传输控制变量
public slots:
	void doWorkUP();					//执行上传操作
	void doWorkDown();					//执行下载操作
signals:
	void workFinished();												//任务完成信号
	void sendItemData(TranPdu data,std::uint64_t fileid);				//发送添加文件视图信号
	void erroroccur(QString message);									//错误信号
	void sendProgress(qint64 bytes, qint64 total);						//发送进度信号
private:
	bool calculateSHA256();				//计算文件哈希值
	bool sendTranPdu();					//发生TranPdu
	bool recvRespon();					//接收服务器回复报文
	bool sendFile();					//发送文件
	bool recvFile();					//接受文件
	bool verifySHA256();	//哈希检查
	void removeFileIfExists();			//移除文件
private:
	SR_Tool m_srTool;		//发送接收工具
	TranPdu m_TranPdu;		//通信结构体
	QString m_filename;		//带路径的文件全名
	QFile   m_file;			//上传或者下载的文件保存数据对象

	ResponsePack m_res;		//服务端回复
	asio::error_code m_ec;	//错误码
	char buf[1024] = { 0 };	//收发数据缓冲区
	
	//上传和下载控制
	std::shared_ptr <std::atomic<int>>m_control;		//原子类型，工作控制.0为继续，1为暂停，2为结束
	std::shared_ptr<std::condition_variable> m_cv;		//共享条件变量
	std::shared_ptr<std::mutex>m_mutex;					//共享互斥锁
};
