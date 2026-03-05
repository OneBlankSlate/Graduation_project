#pragma once

#include <QWidget>
#include "ui_ProcessMemoryWindow.h"
#include<QStandardItemModel>
#include<Windows.h>
#include<QMenu>
class ProcessMemoryWindow : public QWidget
{
	Q_OBJECT

public:
	ProcessMemoryWindow(const QString& ProcessIdentity, QWidget *parent = nullptr);
	~ProcessMemoryWindow();
	void ListProcessMemoryInfo();
	void OpenReadMemWind();
	void OpenWriteMemWind();
	void ModifyProcessProtect(int columnIndex, const QVariant& newValue);  //用于修改内存保护属性的函数
	//菜单
	QMenu* m_TableViewMenu;  //菜单，需要头文件<QMenu>
	QAction* RefreshAct;     //刷新
	QAction* NoAccessAct;    //不可访问
	QAction* ReadAct;        //只读
	QAction* ReadWriteAct;   //读写
	QAction* WriteCopyAct;   //写拷贝
	QAction* ReadExecuteAct; //执行读
	QAction* ReadWriteGuardAct; //读写保护
	QAction* RecoverProtectAct; //保护属性恢复

	HANDLE m_ProcessId;
	ULONG m_OldProtect;   //直接存字符串，便于直接恢复
private:
	Ui::ProcessMemoryWindowClass ui;
	QStandardItemModel m_model;  // 用于管理QTableView的数据模型
private slots:
	void Menu_Slot(QPoint p);
	void RefreshMemory();
	void SetNoAccess();
	void SetRead();
	void SetReadWrite();
	void SetWriteCopy();
	void SetReadExecute();
	void SetReadWriteGuard();
	void RecoverProtect();
};
