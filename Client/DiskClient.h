#pragma once

#include <QtWidgets/QMainWindow>
#include<QFileDialog>
#include<QStandardItemModel>
#include<QFile>
#include<QThread>
#include<QMenu>
#include<QVariant>
#include"ToolClass/SR_Tool.h"
#include"WidgetClass/Login.h"
#include "ui_DiskClient.h"
#include"WidgetClass/UserInfoWidget.h"
#include"ToolClass/ShortTaskManage.h"
#include"WidgetClass/FileSystemView.h"
#include"WidgetClass/TProgress.h"
#include"ToolClass/UdTool.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DiskClientClass; };
QT_END_NAMESPACE

class DiskClient : public QMainWindow
{
    Q_OBJECT

public:
    DiskClient(QWidget* parent = nullptr);
    ~DiskClient();
    bool startClient(); 
    
private:
    void initClient();
    void initUI();
    void RequestCD();  
    bool connectMainServer();
private slots:
    void widgetChaned();
    void on_BtnInfo_clicked();

    void do_createDir(quint64 parentID, QString newdirname);        
    void do_getFileinfo(std::shared_ptr<std::vector<FILEINFO>>vet);
    void do_delFile(quint64 fileif, QString filename);
    void do_btnPuts_clicked();
    void do_Gets_clicked(ItemDate item); 
    void do_error(QString message);                                    
private:
    Ui::DiskClientClass* ui;
    std::shared_ptr<UserInfo>m_userinfo;            //用户信息结构体
    std::shared_ptr<SR_Tool>m_srTool;               //发送和接受数据工具
    std::shared_ptr<TaskQue>m_task;                 //任务队列
    std::shared_ptr<ShortTaskManage>m_taskManage;   //短任务管理器

    FileSystemView* m_FileView = nullptr;             //文件系统视图

    QString CurServerIP;          //连接主服务器后，当前分配的服务器
    int CurSPort=0;               //当前分配的端口
    int CurLPort=0;               //当前分配的长任务端口
};
