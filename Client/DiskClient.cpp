#include "DiskClient.h"

DiskClient::DiskClient(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::DiskClientClass())
{
    ui->setupUi(this);
    m_userinfo = std::make_shared<UserInfo>();                      //保存用户信息
    m_task = std::make_shared<TaskQue>(1);                          //初始化短任务线程池，这里初始化1条线程，如果需要初始化多条，需要处理收发工具的并发问题
}

DiskClient::~DiskClient()
{
    delete ui;
    if (m_srTool->getSsl()->lowest_layer().is_open())
        m_srTool->getSsl()->shutdown();
    m_srTool->getSsl()->lowest_layer().close();
}

//开始客户端，进行登陆或者注册
bool DiskClient::startClient()
{
    bool ret = connectMainServer();
    if (ret) {
        Login login(m_srTool, m_userinfo.get(), this);
        int ret = login.exec();
        if (ret == QDialog::Accepted) {
            initClient();   //初始化UI
            RequestCD();        //请求文件列表，构建文件系统
            return true;
        }
        else
            return false;
    }
    else
        return false;
}


void DiskClient::initClient()
{
    //连接信号
    m_taskManage = std::make_shared<ShortTaskManage>(m_srTool, this);//短任务管理器，用于处理短任务
    connect(m_taskManage.get(), &ShortTaskManage::GetFileListOK, this, &DiskClient::do_getFileinfo);
    connect(m_taskManage.get(), &ShortTaskManage::sig_delOK, this, &DiskClient::RequestCD);
    connect(m_taskManage.get(), &ShortTaskManage::error, this, &DiskClient::do_error);

    initUI();
}

//初始化UI
void DiskClient::initUI()
{
    ui->TranList->setAlignment(Qt::AlignTop);                       //将下载进度界面改成顶部对齐
    m_FileView = new FileSystemView(this);
    ui->FileView->addWidget(m_FileView);
    ui->FileView->setCurrentWidget(m_FileView);

    //连接UI信号槽
    connect(ui->btnFile, &QPushButton::clicked, this, &DiskClient::widgetChaned);
    connect(ui->btnTran, &QPushButton::clicked, this, &DiskClient::widgetChaned);
    connect(m_FileView, &FileSystemView::sig_RefreshView, this, &DiskClient::RequestCD);    //连接刷新视图信号
    connect(m_FileView, &FileSystemView::sig_down, this, &DiskClient::do_Gets_clicked);     //连接下载信号
    connect(m_FileView, &FileSystemView::sig_up, this, &DiskClient::do_btnPuts_clicked);    //连接上传信号
    connect(m_FileView, &FileSystemView::sig_createdir, this, &DiskClient::do_createDir);   //连接创建文件夹信号
    connect(m_FileView, &FileSystemView::sig_deleteFile, this, &DiskClient::do_delFile);    //连接删除文件信号
    connect(m_taskManage.get(), &ShortTaskManage::sig_createDirOK, m_FileView, &FileSystemView::addNewDirItem);   //连接创建文件夹信号
}


//向服务器请求读取文件列表
void DiskClient::RequestCD()
{
    m_task->AddTask(std::bind(&ShortTaskManage::GetFileList, m_taskManage.get()));
}

//连接主服务器（负载均衡器）获得分配的地址和端口
bool DiskClient::connectMainServer()
{
    asio::ip::tcp::endpoint endpoint_(asio::ip::make_address(MainServerIP),MainServerPort); //构建主服务器端点
    asio::io_context io_context_;               //构建IO上下文
    asio::ip::tcp::socket socket_(io_context_); 
    asio::error_code ec;
    socket_.connect(endpoint_, ec);     //开始连接
    if (ec) //连接负载均衡器失败，使用默认地址
    {
        qDebug() << "均衡器无法连接";
    }
    else
    {
        try
        {
            char server_ip[20] = { 0 };
            int sport = 0, lport = 0;

            // 接收服务器IP
            asio::read(socket_, asio::buffer(server_ip, sizeof(server_ip)));
            // 接收服务器sport
            asio::read(socket_, asio::buffer(&sport, sizeof(sport)));
            // 接收服务器lport
            asio::read(socket_, asio::buffer(&lport, sizeof(lport)));
            socket_.close(); // 关闭连接

            CurServerIP = server_ip;
            CurSPort = sport;   // 使用 ntohs 来转换网络字节序
            CurLPort = lport;   // 使用 ntohs 来转换网络字节序

            if (!CurServerIP.isEmpty())
            {
                m_srTool = std::make_shared<SR_Tool>(this, CurServerIP.toStdString(), CurSPort);      //构建收发工具
                return true;
            }
        }
        catch (std::exception& e)
        {
            qDebug() << "均衡器无法连接，使用默认服务器";
        }
    }
    CurServerIP = DefaultserverIP;
    CurSPort = DefaultserverSPort;
    CurLPort = DefaultserverLPort;
    m_srTool = std::make_shared<SR_Tool>(this, CurServerIP.toStdString(), CurSPort);      //构建收发工具
    return true;
}


//上传按钮槽函数,传递要上传到的文件夹ID，默认为0，即根文件夹
void DiskClient::do_btnPuts_clicked()
{
    quint64 parenId = m_FileView->GetCurDirID();
    //创建一个对话框打开文件
    QString filename = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath(), "所有文件(*.*)");
    if (filename.isEmpty())
        return;

    //对文件长度进行判断，服务端不接受太长的文件名
    QFileInfo fileinfo(filename);
    if (fileinfo.fileName().toUtf8().size() >= 100)
    {
        QMessageBox::information(this, "警告", "文件名过长");
        return;
    }

    //以d结尾的服务端默认为文件夹，所有不接受
    QString suffix = fileinfo.suffix(); 
    if (suffix == "d")
    {
        QMessageBox::warning(this, "提示", "后缀名不能为d");
        return;
    }  
    std::string filepath = filename.toStdString();

    //构建通信pdu
    TranPdu pdu;
    pdu.TranPduCode = Code::Puts;
    pdu.parentDirID = parenId;
    memcpy(pdu.user, m_userinfo->username, sizeof(pdu.user));
    memcpy(pdu.pwd, m_userinfo->pwd, sizeof(pdu.pwd));
    memcpy(pdu.filename,filepath.c_str(),filepath.size());
    
    //构建进度条，无需担心会内存泄漏，Qt对象树系统，会自动进入事件循环进行析构
    TProgress* progress = new TProgress(this);
    progress->setFilename(1, filename);
    ui->TranList->addWidget(progress);

    QThread* thread = new QThread();
    UdTool* worker = new UdTool(CurServerIP,CurLPort,nullptr);
    worker->SetTranPdu(pdu);
 
    QObject::connect(worker, &UdTool::workFinished, thread, &QThread::quit);
    QObject::connect(worker, &UdTool::workFinished, worker, &UdTool::deleteLater);
    QObject::connect(worker, &UdTool::erroroccur, thread, &QThread::quit);
    QObject::connect(worker, &UdTool::erroroccur, worker, &UdTool::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    QObject::connect(thread, &QThread::started, worker, &UdTool::doWorkUP);

    //如果使用阻塞连接会导致发送线程阻塞，影响发送效率。可以减少发送次数避免这个问题。
    QObject::connect(worker, &UdTool::sendProgress, progress, &TProgress::do_updateProgress, Qt::QueuedConnection);
    QObject::connect(worker, &UdTool::workFinished, progress, &TProgress::do_finished, Qt::QueuedConnection);
    //QObject::connect(worker, &UdTool::workFinished, this, &DiskClient::RequestCD, Qt::QueuedConnection);    //上传完成，刷新界面。文件太多会导致刷新压力大，不使用。
    QObject::connect(worker, &UdTool::sendItemData, m_FileView, &FileSystemView::addNewFileItem, Qt::QueuedConnection); //添加新上传的项目
    worker->SetControlAtomic(progress->getAtomic(),progress->getCv(),progress->getMutex());
    QObject::connect(worker, &UdTool::erroroccur, this, &DiskClient::do_error);

    // 启动线程
    worker->moveToThread(thread);
    thread->start();
    ui->stackedWidget->setCurrentWidget(ui->two);
}

//执行下载操作
void DiskClient::do_Gets_clicked(ItemDate item)
{
    quint64 FileId = item.ID;
    
    QString filter ="文件类型(*"+ item.filename.right(item.filename.length() - item.filename.lastIndexOf("."))+")";
  

    //创建一个对话框打开文件
    QString filename = QFileDialog::getSaveFileName(this, "选择保存的文件", item.filename, filter);
    if (filename.isEmpty())
        return;

    std::string filepath = filename.toStdString();

    //构建通信pdu
    TranPdu pdu;
    pdu.TranPduCode = Code::Gets;
    pdu.parentDirID = FileId;
    memcpy(pdu.user, m_userinfo->username, sizeof(pdu.user));
    memcpy(pdu.pwd, m_userinfo->pwd, sizeof(pdu.pwd));
    memcpy(pdu.filename, filepath.c_str(), filepath.size());
    pdu.sendedsize = 0;
    pdu.filesize = item.filesize;


    //构建进度条，无需担心会内存泄漏，Qt对象树系统，会自动进入事件循环进行析构
    TProgress* progress = new TProgress(this);
    progress->setFilename(2, filename);
    ui->TranList->addWidget(progress);

    QThread* thread = new QThread();
    UdTool* worker = new UdTool(CurServerIP, CurLPort, nullptr);
    worker->SetTranPdu(pdu);

    QObject::connect(worker, &UdTool::workFinished, thread, &QThread::quit);
    QObject::connect(worker, &UdTool::workFinished, worker, &UdTool::deleteLater);
    QObject::connect(worker, &UdTool::erroroccur, thread, &QThread::quit);
    QObject::connect(worker, &UdTool::erroroccur, worker, &UdTool::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    QObject::connect(thread, &QThread::started, worker, &UdTool::doWorkDown);

    //如果使用阻塞连接会导致发送线程阻塞，影响发送效率。可以减少发送次数避免这个问题。
    QObject::connect(worker, &UdTool::sendProgress, progress, &TProgress::do_updateProgress, Qt::QueuedConnection);
    QObject::connect(worker, &UdTool::workFinished, progress, &TProgress::do_finished, Qt::QueuedConnection);
    worker->SetControlAtomic(progress->getAtomic(), progress->getCv(), progress->getMutex());
    QObject::connect(worker, &UdTool::erroroccur, this, &DiskClient::do_error);

    // 启动线程
    worker->moveToThread(thread);
    thread->start();
    ui->stackedWidget->setCurrentWidget(ui->two);
}


//用户信息按钮按下槽函数：出现窗体显示个人信息
void DiskClient::on_BtnInfo_clicked()
{
    UserInfoWidget *userdialog=new UserInfoWidget(m_userinfo.get(), this);
    userdialog->setAttribute(Qt::WA_DeleteOnClose);
    userdialog->setWindowTitle("用户信息");
    userdialog->show();
}


//往任务队列添加创建文件夹任务
void DiskClient::do_createDir(quint64 parentID, QString newdirname)
{
    m_task->AddTask(std::bind(&ShortTaskManage::MakeDir,m_taskManage.get(), parentID, newdirname));
}

//槽函数接收从服务器发回的文件信息，将其显示到文件视图界面中
void DiskClient::do_getFileinfo(std::shared_ptr<std::vector<FILEINFO>> vet)
{
    m_FileView->InitView(*vet);
}

//删除文件槽函数
void DiskClient::do_delFile(quint64 fileid, QString filename)
{
    m_task->AddTask(std::bind(&ShortTaskManage::del_File, m_taskManage.get(), fileid, filename));
}

//打印错误信息
void DiskClient::do_error(QString message)
{
    QMessageBox box;
    box.resize(200, 100);
    box.warning(this, "Error", message.toUtf8());
}

//主窗口界面变化槽函数
void DiskClient::widgetChaned()
{
    QPushButton *btn=static_cast<QPushButton*>(sender());
    
    QString btnname = btn->objectName();
    if (btnname == "btnFile")
        ui->stackedWidget->setCurrentIndex(0);
    else if (btnname == "btnTran")
        ui->stackedWidget->setCurrentIndex(1);
}


