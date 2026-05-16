#pragma once

#include <Windows.h>
#include <QWidget>
#include "ui_InjectDialog.h"

class InjectDialog : public QDialog
{
	Q_OBJECT

public:
	InjectDialog(DWORD ProcessId, QWidget *parent = nullptr);
	~InjectDialog();

	QString GetDllPath() const;
	int GetInjectMethod() const;

private slots:
	void OnBrowseDll();
	void OnInject();

private:
	Ui::InjectDialogClass ui;
	DWORD m_ProcessId;
};
