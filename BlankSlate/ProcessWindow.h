#pragma once

#include <QWidget>
#include "ui_ProcessWindow.h"
#include <QStandardItemModel>
#include <QMenu>
class ProcessWindow : public QWidget
{
	Q_OBJECT

public:

	ProcessWindow(QWidget *parent = nullptr);
	~ProcessWindow();
	void ListProcessInfo();
	void TerminateProcess();
	void HideProcess();
	void ProtectProcess();
	void UnprotectProcess();
private:
	QMenu* m_TableViewMenu;  //菜单，需要头文件<QMenu>
	QAction* RefreshAct;//菜单项，需要头文件<QAction>
	QAction* ModuleAct;
	QAction* HandleAct;
	QAction* MemoryAct;
	QAction* TerminateProcessAct;
	QAction* HideProcessAct;
	QAction* ProtectProcessAct;
	QAction* UnprotectProcessAct;

	Ui::ProcessWindowClass ui;
	QStandardItemModel m_model;  // 用于管理QTableView的数据模型
public slots:
	//菜单项槽函数
	void Menu_Slot(QPoint p);//右键菜单槽函数
	void RefreshProcess();   //刷新
	void OpenProcessModuleWindow();  //查看进程模块
	void OpenProcessHandleWindow();  //查看进程句柄
	void OpenProcessMemoryWindow();  //查看进程内存
	
};
