#pragma once

#include <QWidget>
#include<QListWidget>
#include<QVariant>
#include<QMenu>
#include<QToolBar>
#include<QDateTime>
#include<QInputDialog>
#include <QAction>
#include <QVBoxLayout>
#include<QLabel>
#include<QDiaLog>
#include "ui_FileSystemView.h"
#include"../function.h"


class FileSystemView : public QWidget
{
	Q_OBJECT

public:
	FileSystemView(QWidget *parent = nullptr);
	~FileSystemView();

    void InitView(std::vector<FILEINFO > &vet);
	quint64 GetCurDirID();												//返回当前目录ID
	void addNewFileItem(TranPdu newItem,std::uint64_t fileid);		    //往文件视图中添加一个新文件项
	void addNewDirItem(quint64 newid, quint64 parentid, QString newdirname);	//往文件视图添加一个新文件夹项
private slots:
	void do_itemDoubleClicked(QListWidgetItem* item);					//项目被双击
	void do_itemClicked(QListWidgetItem* item);							//项目被单击
	void do_createMenu(const QPoint& pos);								//创建鼠标右键菜单
	void do_displayItemData(QListWidgetItem* item);						//鼠标滑进后显示项目信息

	void on_actback_triggered();										//返回行为被按下
	void on_actRefresh_triggered();										//刷新按键按下
	void on_actdown_triggered();										//下载按钮按下
	void on_actupfile_triggered();										//上传按钮按下
	void on_actcreateDir_triggered();									//创建文件夹按钮
	void on_actdelete_triggered();										//删除按钮按下
signals:
	void sig_RefreshView();												//刷新视图信号
	void sig_down(ItemDate data);										//下载信号
	void sig_up();														//上传信号
	void sig_createdir(quint64 parentID,QString newdirname);			//创建文件夹信号
	void sig_deleteFile(quint64 fileif, QString filename);				//删除文件夹信号
private:
	void initIcon(QListWidgetItem* item, QString filetype);
	void initChildListWidget(QListWidget* List);
	quint64 initDirByteSize(QListWidgetItem* item);						//递归更新文件夹空间信息
private:
	Ui::FileSystemViewClass ui;
    QListWidgetItem* currentItem = nullptr; //当前项
	QListWidgetItem* curDirItem = nullptr;	//当前文件夹项

	QListWidget* rootWidegt = nullptr;		//根文件夹界面指针
	QMap<uint64_t, QListWidgetItem*>Map;	//文件夹项ID对应文件夹项指针映射
};
