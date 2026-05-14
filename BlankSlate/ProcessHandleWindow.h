#pragma once
#include <QWidget>
#include "ui_ProcessHandleWindow.h"
#include<QStandardItemModel>
#include<qmenu.h>
#include<Windows.h>
class ProcessHandleWindow : public QWidget
{
	Q_OBJECT

public:
	// 修改构造函数
	ProcessHandleWindow(HANDLE ProcessId, QWidget* parent = nullptr);
	~ProcessHandleWindow();
	void ListProcessHandleInfo(HANDLE ProcessId);
private:
	Ui::ProcessHandleWindowClass ui;
	QStandardItemModel m_model;
	QMenu* m_TableViewMenu;  //菜单，需要头文件<QMenu>
	QAction* CloseHandleAct;
	HANDLE m_ProcessId;
public slots:
	void Menu_Slot(QPoint p);//右键菜单槽函数
	void CloseHandle();

};
