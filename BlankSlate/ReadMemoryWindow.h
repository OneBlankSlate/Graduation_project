#pragma once

#include <QWidget>
#include "ui_ReadMemoryWindow.h"
#include<Windows.h>
#include<iostream>

class ReadMemoryWindow : public QWidget
{
	Q_OBJECT

public:
	ReadMemoryWindow(HANDLE ProcessIdentity,QWidget *parent = nullptr);
	~ReadMemoryWindow();
	BOOL ReadVirtualMemory();
	HANDLE m_ProcessId;
private:
	Ui::ReadMemoryWindowClass ui;
};
