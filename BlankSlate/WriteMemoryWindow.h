#pragma once

#include <QWidget>
#include "ui_WriteMemoryWindow.h"
#include<Windows.h>

class WriteMemoryWindow : public QWidget
{
	Q_OBJECT

public:
	WriteMemoryWindow(HANDLE ProcessIdentity, QWidget *parent = nullptr);
	~WriteMemoryWindow();
	BOOL WriteVirtualMemory();
	HANDLE m_ProcessId;
private:
	Ui::WriteMemoryWindowClass ui;
};
