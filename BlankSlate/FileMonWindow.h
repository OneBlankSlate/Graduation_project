// FileMonWindow.h
#pragma once

#include <QWidget>
#include "ui_FileMonWindow.h"
#include "FileMonCommon.h" // 使用文件事件结构
#include "IoControlHelper.h" // 包含通信相关定义
#include <QStandardItemModel>
#include <QTimer>
#include <windows.h>
#include <psapi.h>
#include <QFileInfo>
class FileMonWindow : public QWidget
{
	Q_OBJECT

public:
	FileMonWindow(QWidget* parent = nullptr);
	~FileMonWindow();

private:
	QStandardItemModel* m_model;
	Ui::FileMonWindowClass* ui;
	QTimer* m_updateTimer;

	void setupTableView();
	void updateUIState(bool isMonitoring);

	// 辅助函数
	QString fileTimeToString(ULONGLONG fileTime);
	QString fileEventTypeToString(FILE_EVENT_TYPE type);

	// 状态变量
	bool m_isMonitoring = false;

private slots:
	void onStartClicked();
	void onStopClicked();
	void onRefreshClicked();
	void onClearClicked();
	void updateEvents();
};