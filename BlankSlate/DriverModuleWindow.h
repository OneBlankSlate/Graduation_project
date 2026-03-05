#pragma once
#include <QWidget>
#include "ui_DriverModuleWindow.h"
#include<QStandardItemModel>
#include<QMenu>
class DriverModuleWindow : public QWidget
{
	Q_OBJECT

public:
	DriverModuleWindow(QWidget *parent = nullptr);
	~DriverModuleWindow();
	void ListDriverModules();
private:
	Ui::DriverModuleWindowClass ui;
	QStandardItemModel m_model;  // 用于管理QTableView的数据模型
	QMenu* m_TableViewMenu;  //菜单，需要头文件<QMenu>
	QAction* RefreshAct;
public slots:
	//菜单项槽函数
	void Menu_Slot(QPoint p);//右键菜单槽函数
	void RefreshDriverModule();   //刷新
};
