#include "UserInfoWidget.h"

UserInfoWidget::UserInfoWidget(UserInfo* info, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::UserInfoWidgetClass())
{
	ui->setupUi(this);
	ui->UserEdit->setText(QString(info->username));
	ui->CapacitySumLine->setText(formatFileSize(QString::fromUtf8(info->capacitySum).toULongLong()));
	ui->CapacityUsedLine->setText(formatFileSize(QString::fromUtf8(info->usedCapacity).toULongLong()));
	ui->VipDateLine->setText(QString(info->vipdate));
	
	if (QString(info->isvip) == "1")
		ui->IsVipLine->setText("是");
	else
		ui->IsVipLine->setText("否");

	ui->UserEdit->setEnabled(false);
	ui->IsVipLine->setEnabled(false);
	ui->CapacitySumLine->setEnabled(false);
	ui->CapacityUsedLine->setEnabled(false);
	ui->VipDateLine->setEnabled(false);
}

UserInfoWidget::~UserInfoWidget()
{
	delete ui;
}
