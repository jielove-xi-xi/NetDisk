#pragma once

#include <QObject>
#include<vector>
#include"SR_Tool.h"
#include"TaskQue.h"

//短任务管理器
class ShortTaskManage  : public QObject
{
	Q_OBJECT

public:
	ShortTaskManage(std::shared_ptr<SR_Tool>srTool,QObject* parent=nullptr);
	~ShortTaskManage();			
public:
	void GetFileList();					//获取文件列表
	void MakeDir(quint64 parentid, QString newdirname);	//创建文件夹
	void del_File(quint64 fileid, QString filename);	//删除文件
signals:
	void GetFileListOK(std::shared_ptr<std::vector<FILEINFO>>vet);
	void error(QString message);
	void sig_createDirOK(quint64 newid,quint64 parentid,QString newdirname);
	void sig_delOK();	
private:
	std::shared_ptr<SR_Tool>m_srTool;
};
