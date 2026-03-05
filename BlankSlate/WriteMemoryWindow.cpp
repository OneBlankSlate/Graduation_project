#include "WriteMemoryWindow.h"
#include"ProcessMemory.h"
WriteMemoryWindow::WriteMemoryWindow(HANDLE ProcessIdentity, QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	m_ProcessId = ProcessIdentity;
	connect(ui.WriteMemOk_Button, &QPushButton::clicked, this, &WriteMemoryWindow::WriteVirtualMemory);
}

WriteMemoryWindow::~WriteMemoryWindow()
{}
BOOL WriteMemoryWindow::WriteVirtualMemory()
{
	BOOL IsOk = FALSE;
	int AddressLength = ui.WriteAddr_LineEdit->text().length();
	if (!AddressLength || AddressLength > 16)
	{
		return IsOk;
	}
	//取地址
	QString value = ui.WriteAddr_LineEdit->text();
	bool Ok = false;
	unsigned long long address = value.toULongLong(&Ok,16);
	PVOID pValue = (PVOID)address;

	//取数据和长度
	QString Data = ui.NewValue_LineEdit->text();
	QByteArray  utfData = Data.toUtf8();
	char* BufferData = (char*)utfData.constData();
	int BufferLength = ui.NewValue_LineEdit->text().length();
	int ViewSize = sizeof(COMMUNICATE_PROCESS_MEMORY) + BufferLength;
	PCOMMUNICATE_PROCESS_MEMORY v5 = (PCOMMUNICATE_PROCESS_MEMORY)malloc(ViewSize);
	if (v5 == NULL)
	{
		return FALSE;
	}
	memset(v5, 0, ViewSize);
	v5->OperateType = WRITE_PROCESS_MEMORY;
	v5->ul.Write.ProcessIdentity = (ULONG_PTR)m_ProcessId;
	v5->ul.Write.BaseAddress = pValue;
	v5->ul.Write.RegionSize = BufferLength;
	v5->ul.Write.BufferData = (char*)(v5 + 1);
	memcpy(v5->ul.Write.BufferData, BufferData, BufferLength);
	IsOk = CommunicateDevice(v5, ViewSize, NULL, 0, NULL);
	return IsOk;
}