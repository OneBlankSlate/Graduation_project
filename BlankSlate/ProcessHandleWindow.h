#pragma once
#include <QWidget>
#include "ui_ProcessHandleWindow.h"
#include<QStandardItemModel>
class ProcessHandleWindow : public QWidget
{
	Q_OBJECT

public:
	ProcessHandleWindow(const QString& ImageName, QWidget *parent = nullptr);
	~ProcessHandleWindow();
	void ListProcessHandleInfo(const wchar_t* ImageName);

private:
	Ui::ProcessHandleWindowClass ui;
	QStandardItemModel m_model;
};
