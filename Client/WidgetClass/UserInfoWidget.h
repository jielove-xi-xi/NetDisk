#pragma once

#include <QDialog>
#include"../function.h"
#include "ui_UserInfoWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class UserInfoWidgetClass; };
QT_END_NAMESPACE

class UserInfoWidget : public QDialog
{
	Q_OBJECT

public:
	UserInfoWidget(UserInfo* info,QWidget *parent = nullptr);
	~UserInfoWidget();

private:
	Ui::UserInfoWidgetClass *ui;
};
