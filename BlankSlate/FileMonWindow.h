// FileMonWindow.h
#pragma once

#include <QWidget>
#include "ui_FileMonWindow.h"
#include "FileMonCommon.h" // 使用文件事件结构
#include "IoControlHelper.h" // 包含通信相关定义
#include "FileMonitor.h"
#include <QStandardItemModel>
#include <QTimer>
#include <windows.h>
#include <psapi.h>
#include <QFileInfo>
#include <QCheckBox>
#include <QListWidget>

// 已保护文件信息
struct ProtectedFileInfo {
	QString dosPath;        // DOS路径（用于显示）
	QString ntPath;         // NT路径（用于驱动通信）
	bool deleteProtected;
	bool modifyProtected;
	bool copyProtected;
};

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
	QPushButton* btnExport;
	QPushButton* btnProtect;
	QList<ProtectedFileInfo> m_protectedFiles;  // 已保护文件列表

	void setupTableView();
	void updateUIState(bool isMonitoring);

	// 辅助函数
	QString fileTimeToString(ULONGLONG fileTime);
	QString fileEventTypeToString(FILE_EVENT_TYPE type);

	// 状态变量
	bool m_isMonitoring = false;

	// 文件保护辅助函数
	bool sendFileProtection(OPERATE_TYPE opType, const QString& ntPath);
	void updateProtectedFileList(const QString& dosPath, const QString& ntPath,
		bool addDelete, bool addModify, bool addCopy,
		bool removeDelete, bool removeModify, bool removeCopy);

private slots:
	void onStartClicked();
	void onStopClicked();
	void onRefreshClicked();
	void onClearClicked();
	void updateEvents();
	void onExportClicked();  //导出选中内容
	void onProtectClicked(); //文件保护
};