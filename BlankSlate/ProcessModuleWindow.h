#pragma once
#include <Windows.h>
#include <QWidget>
#include "ui_ProcessModuleWindow.h"
#include <QStandardItemModel>
#include <QMenu>
class ProcessModuleWindow : public QWidget
{
	Q_OBJECT

public:
	ProcessModuleWindow(DWORD ProcessId, const QString& ImageName, QWidget *parent = nullptr);
	~ProcessModuleWindow();
	void ListProcessModuleInfoByPid();

private:
	Ui::ProcessModuleWindowClass ui;
	QStandardItemModel m_model;
	DWORD m_processId;
	QString m_imageName;

	QMenu* m_TableViewMenu;
	QAction* RefreshAct;
	QAction* UnloadAct;

private slots:
	void Menu_Slot(QPoint p);
	void RefreshModules();
	void UnloadModule();
};
