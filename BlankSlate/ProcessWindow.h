#pragma once

#include <QWidget>
#include "ui_ProcessWindow.h"
#include <QStandardItemModel>
#include <QMenu>
#include<Windows.h>
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
	void hook_NtTerminateProcess();
	void unhook_NtTerminateProcess();
	void hook_NtWriteVirtualMemory();
	void unhook_NtWriteVirtualMemory();
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
	QAction* Hook_NtTerminateProAct;
	QAction* Unhook_NtTerminateProAct;
	QAction* Hook_NtWriteVirtualMemoryAct;
	QAction* Unhook_NtWriteVirtualMemoryAct;

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
// IOCTL控制码定义
#define FILE_DEVICE_ETWHOOK 0x8000

#define IOCTL_PROTECT_TERMINATE \
    CTL_CODE(FILE_DEVICE_ETWHOOK, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PROTECT_WRITE \
    CTL_CODE(FILE_DEVICE_ETWHOOK, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// 保护配置结构
typedef struct _PROTECT_CONFIG {
	WCHAR ProcessName[256];  // 进程名
	BOOLEAN IsProtected;     // 是否受保护
} PROTECT_CONFIG, * PPROTECT_CONFIG;
