#include "ReadMemoryWindow.h"
#include"ProcessMemory.h"
#include"WriteMemoryWindow.h"

ReadMemoryWindow::ReadMemoryWindow(HANDLE ProcessIdentity, QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	m_ProcessId = ProcessIdentity;
	connect(ui.ReadMemOk_Button, &QPushButton::clicked, this, &ReadMemoryWindow::ReadVirtualMemory);
	
}

ReadMemoryWindow::~ReadMemoryWindow()
{}

BOOL ReadMemoryWindow::ReadVirtualMemory()
{
	BOOL IsOk = FALSE;
	int AddressLength = ui.ReadAddr_LineEdit->text().length();
	if (!AddressLength || AddressLength > 16)
	{
		return IsOk;
	}
	ui.AddrValue_LineEdit->setReadOnly(true);
	// 获取用户输入的文本并转换为整数
	QString value1 = ui.ReadAddr_LineEdit->text();
	bool Ok1 = false;
	unsigned long long  address = value1.toULongLong(&Ok1,16);

	QString value2 = ui.ReadSize_LineEdit->text();
	bool Ok2 = false;
	unsigned long long  size = value2.toULongLong(&Ok2, 16);
	// 将整数的地址赋给PVOID类型的变量
	PVOID pValue = (PVOID)address;
	COMMUNICATE_PROCESS_MEMORY v5;
	v5.OperateType = READ_PROCESS_MEMORY;
	v5.ul.Read.ProcessIdentity = (ULONG_PTR)m_ProcessId;
	v5.ul.Read.BaseAddress = pValue;
	v5.ul.Read.RegionSize = size;

	DWORD ReturnLength = 0;
	char BufferData[MAX_LENGTH] = { 0 };
	IsOk = CommunicateDevice(&v5, sizeof(COMMUNICATE_PROCESS_MEMORY), BufferData, MAX_LENGTH, &ReturnLength);
	if (IsOk)
	{
		QByteArray data((char*)BufferData, size);
		QString hexResult = data.toHex().toUpper();
		ui.AddrValue_LineEdit->setText(hexResult);       //给控件赋值
	}
	return IsOk;
}
