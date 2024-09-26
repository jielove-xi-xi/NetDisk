#include "FileSystemView.h"

FileSystemView::FileSystemView(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);	
	this->setWindowFlag(Qt::FramelessWindowHint);

	QVBoxLayout* MainLayout = new QVBoxLayout(this);
	QToolBar* tool = new QToolBar(this);

	tool->addAction(ui.actback);
	tool->addAction(ui.actdown);
	tool->addAction(ui.actupfile);
	tool->addAction(ui.actdelete);
	tool->addAction(ui.actcreateDir);
	tool->addAction(ui.actRefresh);
	tool->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

	MainLayout->addWidget(tool);
	MainLayout->addWidget(ui.RootWidget);
	MainLayout->setSpacing(0);
	MainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(MainLayout);

	this->setContentsMargins(0, 0, 0, 0);
}

FileSystemView::~FileSystemView()
{
	
}

void FileSystemView::InitView(std::vector<FILEINFO>&vet)
{
	quint64 preID = 0;							//用于界面刷新，恢复上次的选项
	if (curDirItem!=nullptr) {
		preID = curDirItem->data(Qt::UserRole).value<ItemDate>().ID;	//暂存该ID
	}
	

	QListWidget *newrootWidegt = new QListWidget(this);				//创建新根目录
	initChildListWidget(newrootWidegt);								//初始化根目录
	
	if (vet.size() == 0) {		//如果用户文件列表为空
		rootWidegt = newrootWidegt;
		ui.RootWidget->setCurrentWidget(rootWidegt);
		return;
	}


	Map.clear();	//清楚旧数据
	for (const FILEINFO& info : vet)
	{
		ItemDate data;
		data.ID = info.FileId;
		data.filename = info.Filename;
		data.filetype = info.FileType;
		data.Grade = info.DirGrade;
		data.parentID = info.ParentDir;
		data.filesize = info.FileSize;
		data.filedate = info.FileDate;

		QListWidgetItem* item = new QListWidgetItem();	//生成一个Item

		ItemDate parent;
		if (data.Grade != 0)	//如果不是根节点,拿到它的父节点的ItemData
			parent = Map.value(info.ParentDir)->data(Qt::UserRole).value<ItemDate>();

		if (data.Grade == 0)	//如果是根节点的项目，将其挂在根目录下
		{
			newrootWidegt->addItem(item);
			data.parent = newrootWidegt;
		}
		else
		{
			if (parent.child) 
			{
				parent.child->addItem(item);		//挂在子节点之下
				data.parent = parent.child;			//将其指向父节点的子文件夹
			}
		}

		if (data.filetype == "d")					//如果是目录，需要创建子目录指针
		{
			if (data.Grade == 0)
				data.child = new QListWidget(newrootWidegt);	//挂在根目录下
			else
				data.child = new QListWidget(parent.child);		//挂在父目录下
		}

		QVariant va = QVariant::fromValue(data);		//保存数据
		item->setData(Qt::UserRole, va);				
		initIcon(item, data.filetype);
		item->setText(data.filename);
		
		if (data.filetype == "d") {				//只需要添加文件夹项
			initChildListWidget(data.child);	//初始化子ListWidget
			Map.insert(info.FileId, item);		//添加金Map
		}
	}
	//删除旧数据
	if (rootWidegt) {
		rootWidegt->clear();
		delete rootWidegt;	//删除该对象以及子对象。之所以不使用deleteLater()怕刷新频繁，造成内存积累
		rootWidegt = nullptr;
	}
	
	rootWidegt = newrootWidegt;

	QListWidgetItem* curwidget = Map.value(preID,nullptr);		//如果之前的文件夹项目还在，重新指向该界面
	if (curwidget!=nullptr) {
		ItemDate cur = curwidget->data(Qt::UserRole).value<ItemDate>();
		ui.RootWidget->setCurrentWidget(cur.child);	//如何还存在，设置为当前窗体
		curDirItem = curwidget;
	}
	else {
		ui.RootWidget->setCurrentWidget(rootWidegt);//否则设置根窗体
		curDirItem = nullptr;
	}
	for (int i = 0; i < rootWidegt->count(); i++)	//更新文件夹空间信息，文件过多，可能会影响性能。
	{
		initDirByteSize(rootWidegt->item(i));
	}
}

//返回当前目录ID
quint64 FileSystemView::GetCurDirID()
{
	if (curDirItem == nullptr)	//如果是根目录
		return 0;

	ItemDate data = curDirItem->data(Qt::UserRole).value<ItemDate>();
	return data.ID;
}

//往文件视图添加一个新文件项
void FileSystemView::addNewFileItem(TranPdu newItem, std::uint64_t fileid)
{
	//构建项
	ItemDate data;
	data.ID = fileid;		//user保存着服务端返回的文件ID，转换为数字
	data.filename = newItem.filename;
	data.filetype=QString(newItem.filename).section('.', -1, -1);
	data.parentID = newItem.parentDirID;
	data.filesize = newItem.filesize;
	QDateTime currentTime = QDateTime::currentDateTime();  // 获取当前系统时间
	data.filedate = currentTime.toString("yyyy-MM-dd hh:mm:ss");

	QListWidgetItem* item=new QListWidgetItem();
	initIcon(item, data.filetype);
	item->setText(data.filename);
	//获取其父节点的项对象
	QListWidgetItem* parent = Map.value(data.parentID,nullptr);

	if (parent == nullptr) {	//不存在父节点，挂在根节点上
		rootWidegt->addItem(item);
		data.Grade = 0;			//级别为0
	}
	else {
		ItemDate parentdata = parent->data(Qt::UserRole).value<ItemDate>();	//挂在父节点上
		parentdata.child->addItem(item);
		data.Grade = parentdata.Grade + 1;	//父级别+1
	}
	QVariant va = QVariant::fromValue(data);	//设置数据
	item->setData(Qt::UserRole, va);

	//向上递归更新文件夹空间
	while (parent)		//只要父节点不为空，继续向上
	{
		ItemDate curdata = parent->data(Qt::UserRole).value<ItemDate>();
		curdata.filesize += data.filesize;
		parent->setData(Qt::UserRole, QVariant::fromValue(curdata));	//更新父节点数据
		parent = Map.value(curdata.parentID, nullptr);	//向上递归
	}
}


//往文件视图添加一个新文件夹项
void FileSystemView::addNewDirItem(quint64 newid, quint64 parentid, QString newdirname)
{
	//构建项
	ItemDate data;
	data.ID = newid;		//user保存着服务端返回的文件ID
	data.filename = newdirname;
	data.filetype = "d";
	data.parentID = parentid;
	data.filesize = 0;
	QDateTime currentTime = QDateTime::currentDateTime();  // 获取当前系统时间
	data.filedate = currentTime.toString("yyyy-MM-dd hh:mm:ss");

	QListWidgetItem* item = new QListWidgetItem();
	initIcon(item, data.filetype);
	item->setText(data.filename);
	//获取其父节点的项对象
	QListWidgetItem* parent = Map.value(data.parentID, nullptr);

	if (parent == nullptr) {	//不存在父节点，挂在根节点上
		rootWidegt->addItem(item);
		data.child = new QListWidget(rootWidegt);	//子项指针挂在父节点子项指针上
		data.Grade = 0;			//级别为0
	}
	else {
		ItemDate parentdata = parent->data(Qt::UserRole).value<ItemDate>();	//挂在父节点上
		parentdata.child->addItem(item);
		data.child = new QListWidget(parentdata.child);
		data.Grade = parentdata.Grade + 1;	//父级别+1
	}

	QVariant va = QVariant::fromValue(data);	//设置数据
	item->setData(Qt::UserRole, va);

	initChildListWidget(data.child);	//初始化子项目指针
}

//项目被双击
void FileSystemView::do_itemDoubleClicked(QListWidgetItem* item)
{
	if (item == nullptr)return;
	ItemDate data = item->data(Qt::UserRole).value<ItemDate>();

	if (data.filetype == "d")	//如果是文件夹
	{
		curDirItem = item;
		ui.RootWidget->setCurrentWidget(data.child);
		ui.actdown->setEnabled(false);
	}
	currentItem = item;
}

void FileSystemView::do_itemClicked(QListWidgetItem* item)
{
	if (item == nullptr)return;
	ItemDate data = item->data(Qt::UserRole).value<ItemDate>();

	if (data.filetype != "d")	//如果是文件夹
		ui.actdown->setEnabled(true);
	else
		ui.actdown->setEnabled(false);
	currentItem = item;
}

//创建右键菜单,不同项显示不是菜单
void FileSystemView::do_createMenu(const QPoint& pos)
{
	QListWidget* list = static_cast<QListWidget*>(sender());
	if (list == nullptr)return;
	QMenu menu(this);	//创建菜单
	ItemDate data;
	QListWidgetItem *item=list->itemAt(pos);
	if(item!=nullptr)
		data = item->data(Qt::UserRole).value<ItemDate>();
	if (item == nullptr)
	{
		menu.addAction(ui.actback);
		menu.addAction(ui.actupfile);
		menu.addAction(ui.actRefresh);
		menu.addAction(ui.actcreateDir);
	}
	else if (data.filetype == 'd')		//如果是文件夹
	{
		menu.addAction(ui.actdelete);
	}
	else
	{
		menu.addAction(ui.actdelete);
		menu.addAction(ui.actdown);
	}
	menu.exec(QCursor::pos());
}

void FileSystemView::do_displayItemData(QListWidgetItem* item)
{
	if (item == nullptr)return;

	ItemDate data = item->data(Qt::UserRole).value<ItemDate>();
	if (data.filetype == "d")
		data.filetype = "文件夹";

	// 格式化信息
	QString info = QString("文件名称:  %1\n文件类型:  %2\n文件大小:  %3 bytes\n上传日期:  %4")
		.arg(data.filename)
		.arg(data.filetype)
		.arg(formatFileSize(data.filesize))
		.arg(data.filedate);

	item->setToolTip(info);
}

//返回行为被按下
void FileSystemView::on_actback_triggered()
{
	if (curDirItem == nullptr)return;

	ItemDate data= curDirItem->data(Qt::UserRole).value<ItemDate>();

	if(data.parentID==0)
		ui.RootWidget->setCurrentWidget(rootWidegt);
	else
		ui.RootWidget->setCurrentWidget(data.parent);	//设置为前目录
	curDirItem = Map.value(data.parentID, nullptr);//修改当前文件夹
}

//刷新行为被按下
void FileSystemView::on_actRefresh_triggered()
{
	emit sig_RefreshView();	//发送信号
}

//下载信号
void FileSystemView::on_actdown_triggered()
{
	if (currentItem == nullptr)return;


	ItemDate data = currentItem->data(Qt::UserRole).value<ItemDate>();
	if (data.filetype == "d")
		return;
	else
		emit sig_down(data);
}

//上传行为按下
void FileSystemView::on_actupfile_triggered()
{
	emit sig_up();
}

//创建文件夹按钮按下
void FileSystemView::on_actcreateDir_triggered()
{
	QInputDialog dialog(this);
	dialog.setMinimumSize(100, 100);  // 设置最小尺寸
	dialog.setMaximumSize(200, 200);  // 设置最大尺寸
	QString newdirname = dialog.getText(nullptr, "请输入新文件夹名", "文件夹名称:");
	if (newdirname.isEmpty())
		return;
	qint64 parentid = 0;
	if (curDirItem == nullptr)
	{
		parentid = 0;
	}
	else
	{
		ItemDate data = curDirItem->data(Qt::UserRole).value<ItemDate>();
		parentid = data.ID;
	}

	emit sig_createdir(parentid, newdirname);
}

//删除按钮按下
void FileSystemView::on_actdelete_triggered()
{
	if (currentItem == nullptr)return;
	ItemDate data = currentItem->data(Qt::UserRole).value<ItemDate>();

	quint64 fileid = data.ID;
	QString filename = data.filename;
	if (data.filetype == "d")
	{
		filename = "dir.d";		//服务端删除文件不需要文件名，只需要ID和后缀名。所以只需要传递后缀名可以
	}
	else
	{
		filename = "file.other";
	}
	emit sig_deleteFile(fileid, filename);
}

//设置图标
void FileSystemView::initIcon(QListWidgetItem* item, QString type)
{
	if (type == "d")
		item->setIcon(QIcon(":/icon/icon/dirIcon.svg"));
	else if (type == "jpg")
		item->setIcon(QIcon(":/icon/icon/pitureIcon.svg"));
	else if (type == "mp4")
		item->setIcon(QIcon(":/icon/icon/MP4Icon.svg"));
	else if (type == "txt")
		item->setIcon(QIcon(":/icon/icon/txtIcon.svg"));
	else if (type == "mp3")
		item->setIcon(QIcon(":/icon/icon/mp3Icon.svg"));
	else
		item->setIcon(QIcon(":/icon/icon/otherIcon.svg"));
}

//初始化每个QlistWidget属性，和连接信号
void FileSystemView::initChildListWidget(QListWidget* List)
{
	List->setViewMode(QListView::IconMode);	//设置图标显示模式
	List->setFlow(QListView::LeftToRight);	//设置图标排列从在到右,从上到下
	List->setSpacing(20);

	List->setResizeMode(QListView::Adjust);		//设置随父窗体变大变小
	List->setContextMenuPolicy(Qt::CustomContextMenu);	//设置自定义内容菜单
	List->setMouseTracking(true);						//设置鼠标跟踪
	List->setDragEnabled(false);

	ui.RootWidget->addWidget(List);			//将其添加到堆栈布局窗体
	connect(List, &QListWidget::itemDoubleClicked, this, &FileSystemView::do_itemDoubleClicked);
	connect(List, &QListWidget::customContextMenuRequested, this, &FileSystemView::do_createMenu);
	connect(List, &QListWidget::itemEntered, this, &FileSystemView::do_displayItemData);
	connect(List, &QListWidget::itemClicked, this, &FileSystemView::do_itemClicked);
}

quint64 FileSystemView::initDirByteSize(QListWidgetItem* item)
{
	if (!item) {
		return 0;
	}

	// 获取当前项目的数据
	ItemDate data = item->data(Qt::UserRole).value<ItemDate>();
	quint64 totalSize = data.filesize;  // 初始化为当前文件的大小

	// 如果当前项目是一个文件夹，递归计算其子项的大小
	if (data.child) {
		for (int i = 0; i < data.child->count(); ++i) {
			QListWidgetItem* childItem = data.child->item(i);
			totalSize += initDirByteSize(childItem);
		}
	}

	// 更新当前项的大小并保存回 QListWidgetItem
	data.filesize = totalSize;
	item->setData(Qt::UserRole, QVariant::fromValue(data));

	return totalSize;
}
