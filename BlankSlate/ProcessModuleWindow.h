#pragma once
#include <QWidget>
#include "ui_ProcessModuleWindow.h"
#include <QStandardItemModel>
class ProcessModuleWindow : public QWidget
{
	Q_OBJECT

public:
	ProcessModuleWindow(const QString& ImageName,QWidget *parent = nullptr);
	~ProcessModuleWindow();
	void ListProcessModuleInfo(const wchar_t* ImageName);
	
private:
	Ui::ProcessModuleWindowClass ui;
	QStandardItemModel m_model;
};
