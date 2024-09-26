#include "TProgress.h"

TProgress::TProgress(QWidget* parent)
	: QDialog(parent), m_control(std::make_shared<std::atomic<int>>(0)), m_option(0)
	, m_cv(std::make_shared<std::condition_variable>()), m_mutex(std::make_shared<std::mutex>())
{
	ui.setupUi(this);
}

TProgress::~TProgress()
{
	
}

//输入上传还是下载，设置文件名.option为1为上传，2为下载
void TProgress::setFilename(const int &option, const QString & filename)
{
	m_filename = filename;
	m_option = option;
	if (m_option == 1) {
		ui.FileTitle->setText("正在上传：" + filename);
	}
	else
	{
		ui.FileTitle->setText("正在下载：" + filename);
	}
}

//返回控制原子
std::shared_ptr <std::atomic<int>> TProgress::getAtomic()
{
	return m_control;
}

//返回控制条件变量
std::shared_ptr<std::condition_variable> TProgress::getCv()
{
	return m_cv;
}

//返回控制互斥量
std::shared_ptr<std::mutex> TProgress::getMutex()
{
	return m_mutex;
}

//继续按钮
void TProgress::on_btncontine_clicked()
{
	if (m_option == 1) {
		ui.FileTitle->setText("正在上传：" + m_filename);
	}
	else
	{
		ui.FileTitle->setText("正在下载：" + m_filename);
	}
	m_control->store(0);
	m_cv->notify_one();
}

//结束按钮
void TProgress::on_btnclose_clicked()
{
	QMessageBox::StandardButton reply;
	if (ui.progressBar->value() != ui.progressBar->maximum()) {
		reply = QMessageBox::question(this, tr("请确认"), tr("确定结束？"),
			QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes)
		{
			m_control->store(2);
			m_cv->notify_one();
			this->deleteLater();	//进入事件循环，释放自己，避免资源泄漏
		}
	}
	else
	{
		this->deleteLater();	//进入事件循环，释放自己，避免资源泄漏
	}
}

//进度达到100%槽函数
void TProgress::do_finished()
{
	if (m_option == 1)
		ui.FileTitle->setText("上传：" + m_filename + " 成功");
	else
		ui.FileTitle->setText("下载：" + m_filename + " 成功");
}

//更新进度显示
void TProgress::do_updateProgress(qint64 bytes, qint64 total)
{
	if (total > INT_MAX) {
		// 按比例缩放
		double ratio = (double)bytes / total;  // 计算比例
		int scaledValue = static_cast<int>(ratio * INT_MAX);  // 将比例转为int范围
		ui.progressBar->setMaximum(INT_MAX);
		ui.progressBar->setValue(scaledValue);
	}
	else {
		// 正常处理
		ui.progressBar->setMaximum(static_cast<int>(total));
		ui.progressBar->setValue(static_cast<int>(bytes));
	}
}

//暂停按钮
void TProgress::on_btnstop_clicked()
{
	m_control->store(1);
	ui.FileTitle->setText("已暂停");
}
