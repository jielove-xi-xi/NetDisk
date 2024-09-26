#pragma once

#include<QMessageBox>
#include"../function.h"
#include "ui_TProgress.h"
#include <condition_variable>

class TProgress : public QDialog
{
	Q_OBJECT

public:
	TProgress(QWidget *parent = nullptr);
	~TProgress();
	void setFilename(const int& option, const QString& filename);		
	std::shared_ptr <std::atomic<int>> getAtomic();
	std::shared_ptr<std::condition_variable>getCv();
	std::shared_ptr<std::mutex>getMutex();
public slots:
	void do_updateProgress(qint64 bytes, qint64 total);		//更新下载和上传进度
	void do_finished();				//进度达到100%槽函数
private slots:
	void on_btnstop_clicked();		//暂停键按下
	void on_btncontine_clicked();	//继续键按下
	void on_btnclose_clicked();		//结束键按下
private:
	Ui::TProgressClass ui;
	QString m_filename;			//文件名
	int m_option;				//区分上传还是下载，1上传，2下载

	//下载控制
	std::shared_ptr <std::atomic<int>>m_control;							//原子类型控制量
	std::shared_ptr<std::condition_variable> m_cv;		//共享条件变量
	std::shared_ptr<std::mutex>m_mutex;					//共享互斥锁

};
