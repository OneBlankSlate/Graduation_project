#include "ProcessMemoryWindow.h"
#include<QAbstractItemView>
#include"ProcessMemory.h"
#include"ReadMemoryWindow.h"
#include"WriteMemoryWindow.h"

ProcessMemoryWindow::ProcessMemoryWindow(const QString& ProcessIdentity,QWidget *parent) : QWidget(parent)
{

	ui.setupUi(this);
	m_OldProtect = 0;

	 //列表属性
	ui.ProcessMemory_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // 设置选择行为为整行选中
	ui.ProcessMemory_TableView->setContextMenuPolicy(Qt::CustomContextMenu); //可弹出右键菜单  必须设置
	ui.ProcessMemory_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);//不可编辑
	m_model.setColumnCount(5); // 设置列数为5
	// 设置表头
	QStringList headers = { QStringLiteral("地址"), QStringLiteral("大小"), QStringLiteral("Protect"),QStringLiteral("State"),QStringLiteral("Type")};
	m_model.setHorizontalHeaderLabels(headers);
	// 将模型设置到视图
	ui.ProcessMemory_TableView->setModel(&m_model);  //只用设置这一次，关联上之后，以后直接操作m_model就行了
	ui.ProcessMemory_TableView->show();
	//列内存
	m_ProcessId = (HANDLE)ProcessIdentity.toULongLong();
	ListProcessMemoryInfo();
	//连接读写的槽函数
	connect(ui.ReadMemory_Button, &QPushButton::clicked, this, &ProcessMemoryWindow::OpenReadMemWind);
	connect(ui.WriteMemory_Button, &QPushButton::clicked, this, &ProcessMemoryWindow::OpenWriteMemWind);


	//添加菜单项
	m_TableViewMenu = new QMenu(ui.ProcessMemory_TableView);
	RefreshAct = new QAction(QStringLiteral("刷新"), ui.ProcessMemory_TableView);
	NoAccessAct = new QAction(QStringLiteral("修改为-No Acccess"), ui.ProcessMemory_TableView);
	ReadAct = new QAction(QStringLiteral("修改为-Read"), ui.ProcessMemory_TableView);
	ReadWriteAct = new QAction(QStringLiteral("修改为-ReadWrite"), ui.ProcessMemory_TableView);
	WriteCopyAct = new QAction(QStringLiteral("修改为-WriteCopy"), ui.ProcessMemory_TableView);
	ReadExecuteAct = new QAction(QStringLiteral("修改为-ReadExecute"), ui.ProcessMemory_TableView);
	ReadWriteGuardAct = new QAction(QStringLiteral("修改为-ReadWriteGuard"), ui.ProcessMemory_TableView);
	RecoverProtectAct = new QAction(QStringLiteral("恢复保护属性"), ui.ProcessMemory_TableView);
	m_TableViewMenu->addAction(RefreshAct);
	m_TableViewMenu->addAction(NoAccessAct);
	m_TableViewMenu->addAction(ReadAct);
	m_TableViewMenu->addAction(ReadWriteAct);
	m_TableViewMenu->addAction(WriteCopyAct);
	m_TableViewMenu->addAction(ReadExecuteAct);
	m_TableViewMenu->addAction(ReadWriteGuardAct);
	m_TableViewMenu->addAction(RecoverProtectAct);
	//消息关联
	connect(ui.ProcessMemory_TableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(Menu_Slot(QPoint)));  //菜单初始化
	connect(RefreshAct, &QAction::triggered, this, &ProcessMemoryWindow::RefreshMemory);
	connect(NoAccessAct, &QAction::triggered, this, &ProcessMemoryWindow::SetNoAccess);
	connect(ReadAct, &QAction::triggered, this, &ProcessMemoryWindow::SetRead);
	connect(ReadWriteAct, &QAction::triggered, this, &ProcessMemoryWindow::SetReadWrite);
	connect(WriteCopyAct, &QAction::triggered, this, &ProcessMemoryWindow::SetWriteCopy);
	connect(ReadExecuteAct, &QAction::triggered, this, &ProcessMemoryWindow::SetReadExecute);
	connect(ReadWriteGuardAct, &QAction::triggered, this, &ProcessMemoryWindow::SetReadWriteGuard);
	connect(RecoverProtectAct, &QAction::triggered, this, &ProcessMemoryWindow::RecoverProtect);

	
}
ProcessMemoryWindow::~ProcessMemoryWindow()
{
}
void ProcessMemoryWindow::Menu_Slot(QPoint p)
{
	QModelIndex index = ui.ProcessMemory_TableView->indexAt(p);//获取鼠标点击位置项的索引
	if (index.isValid())//数据项是否有效，空白处点击无菜单
	{
		//显示菜单
		QItemSelectionModel* selections = ui.ProcessMemory_TableView->selectionModel();//获取当前的选择模型
		QModelIndexList selected = selections->selectedIndexes();//返回当前选择的模型索引
		if (selected.count() == 1) //选择单个项目时的右键菜单显示Action1
		{
			//RefreshAct->setVisible(true);
			m_TableViewMenu->setVisible(true);
		}
		else   //如果选中多个项目，则右键菜单显示Action2
		{
			//RefreshAct->setVisible(true);
			m_TableViewMenu->setVisible(true);
		}
		m_TableViewMenu->exec(QCursor::pos());//数据项有效才显示菜单
	}
	
}


void ProcessMemoryWindow::RefreshMemory()
{
	m_model.clear();
	ListProcessMemoryInfo();
}
void ProcessMemoryWindow::SetNoAccess()
{
	//获取所选行的BaseAddress和RegionSize
	QItemSelectionModel* selectionModel = ui.ProcessMemory_TableView->selectionModel();
	QModelIndex selectedRow = selectionModel->selectedRows().first();  	// 获取选中行的第一个索引
	QModelIndex baseAddressIndex = selectedRow.sibling(selectedRow.row(), 0); // 获取指定列的索引   baseaddress
	QModelIndex regionSizeIndex = selectedRow.sibling(selectedRow.row(), 1);  // RegionSize
	QString address = baseAddressIndex.data().toString();
	QString Size = regionSizeIndex.data().toString();
	bool Ok = false;
	PVOID BaseAddress = (PVOID)address.toULongLong(&Ok, 16);
	SIZE_T RegionSize = Size.toULongLong(&Ok, 16);
	//通信
	BOOL IsOk = FALSE;
	DWORD ReturnValue = 0;
	COMMUNICATE_PROCESS_MEMORY v5;
	memset(&v5, 0, sizeof(COMMUNICATE_PROCESS_MEMORY));
	v5.OperateType = MODIFY_PROCESS_MEMORY;
	v5.Modify.ProcessIdentity = (ULONG_PTR)m_ProcessId;
	v5.Modify.BaseAddress = BaseAddress;
	v5.Modify.RegionSize = RegionSize;
	v5.Modify.NewProtect = 1;  //No Access
	IsOk = CommunicateDevice(&v5, sizeof(COMMUNICATE_PROCESS_MEMORY), &v5.Modify.OldProtect, sizeof(ULONG), &ReturnValue);
	if (IsOk)
	{
		ModifyProcessProtect(2, QStringLiteral("No Access"));  //2号列为Protect属性
		if (m_OldProtect == 0)
		{
			m_OldProtect = v5.Modify.OldProtect;
		}
	}
	
}
void ProcessMemoryWindow::SetRead()
{
	//获取所选行的BaseAddress和RegionSize
	QItemSelectionModel* selectionModel = ui.ProcessMemory_TableView->selectionModel();
	QModelIndex selectedRow = selectionModel->selectedRows().first();  	// 获取选中行的第一个索引
	QModelIndex baseAddressIndex = selectedRow.sibling(selectedRow.row(), 0); // 获取指定列的索引   baseaddress
	QModelIndex regionSizeIndex = selectedRow.sibling(selectedRow.row(), 1);  // RegionSize
	QString address = baseAddressIndex.data().toString();
	QString Size = regionSizeIndex.data().toString();
	bool Ok = false;
	PVOID BaseAddress = (PVOID)address.toULongLong(&Ok, 16);
	SIZE_T RegionSize = Size.toULongLong(&Ok, 16);
	//通信
	BOOL IsOk = FALSE;
	DWORD ReturnValue = 0;
	COMMUNICATE_PROCESS_MEMORY v5;
	memset(&v5, 0, sizeof(COMMUNICATE_PROCESS_MEMORY));
	v5.OperateType = MODIFY_PROCESS_MEMORY;
	v5.Modify.ProcessIdentity = (ULONG_PTR)m_ProcessId;
	v5.Modify.BaseAddress = BaseAddress;
	v5.Modify.RegionSize = RegionSize;
	v5.Modify.NewProtect = 2;  //Read
	IsOk = CommunicateDevice(&v5, sizeof(COMMUNICATE_PROCESS_MEMORY), &v5.Modify.OldProtect, sizeof(ULONG), &ReturnValue);
	if (IsOk)
	{
		ModifyProcessProtect(2, QStringLiteral("Read"));  //2号列为Protect属性
		if (m_OldProtect == 0)
		{
			m_OldProtect = v5.Modify.OldProtect;
		}
	}
	
}
void ProcessMemoryWindow::SetReadWrite()
{
	//获取所选行的BaseAddress和RegionSize
	QItemSelectionModel* selectionModel = ui.ProcessMemory_TableView->selectionModel();
	QModelIndex selectedRow = selectionModel->selectedRows().first();  	// 获取选中行的第一个索引
	QModelIndex baseAddressIndex = selectedRow.sibling(selectedRow.row(), 0); // 获取指定列的索引   baseaddress
	QModelIndex regionSizeIndex = selectedRow.sibling(selectedRow.row(), 1);  // RegionSize
	QString address = baseAddressIndex.data().toString();
	QString Size = regionSizeIndex.data().toString();
	bool Ok = false;
	PVOID BaseAddress = (PVOID)address.toULongLong(&Ok, 16);
	SIZE_T RegionSize = Size.toULongLong(&Ok, 16);
	//通信
	BOOL IsOk = FALSE;
	DWORD ReturnValue = 0;
	COMMUNICATE_PROCESS_MEMORY v5;
	memset(&v5, 0, sizeof(COMMUNICATE_PROCESS_MEMORY));
	v5.OperateType = MODIFY_PROCESS_MEMORY;
	v5.Modify.ProcessIdentity = (ULONG_PTR)m_ProcessId;
	v5.Modify.BaseAddress = BaseAddress;
	v5.Modify.RegionSize = RegionSize;
	v5.Modify.NewProtect = 4;  //ReadWrite

	IsOk = CommunicateDevice(&v5, sizeof(COMMUNICATE_PROCESS_MEMORY), &v5.Modify.OldProtect, sizeof(ULONG), &ReturnValue);
	if (IsOk)
	{
		ModifyProcessProtect(2, QStringLiteral("ReadWrite"));  //2号列为Protect属性
		if (m_OldProtect == 0)
		{
			m_OldProtect = v5.Modify.OldProtect;
		}
	}

}
void ProcessMemoryWindow::SetWriteCopy()
{
	//获取所选行的BaseAddress和RegionSize
	QItemSelectionModel* selectionModel = ui.ProcessMemory_TableView->selectionModel();
	QModelIndex selectedRow = selectionModel->selectedRows().first();  	// 获取选中行的第一个索引
	QModelIndex baseAddressIndex = selectedRow.sibling(selectedRow.row(), 0); // 获取指定列的索引   baseaddress
	QModelIndex regionSizeIndex = selectedRow.sibling(selectedRow.row(), 1);  // RegionSize
	QString address = baseAddressIndex.data().toString();
	QString Size = regionSizeIndex.data().toString();
	bool Ok = false;
	PVOID BaseAddress = (PVOID)address.toULongLong(&Ok, 16);
	SIZE_T RegionSize = Size.toULongLong(&Ok, 16);
	//通信
	BOOL IsOk = FALSE;
	DWORD ReturnValue = 0;
	COMMUNICATE_PROCESS_MEMORY v5;
	memset(&v5, 0, sizeof(COMMUNICATE_PROCESS_MEMORY));
	v5.OperateType = MODIFY_PROCESS_MEMORY;
	v5.Modify.ProcessIdentity = (ULONG_PTR)m_ProcessId;
	v5.Modify.BaseAddress = BaseAddress;
	v5.Modify.RegionSize = RegionSize;
	v5.Modify.NewProtect = 8;  //WriteCopy

	IsOk = CommunicateDevice(&v5, sizeof(COMMUNICATE_PROCESS_MEMORY), &v5.Modify.OldProtect, sizeof(ULONG), &ReturnValue);
	if (IsOk)
	{
		ModifyProcessProtect(2, QStringLiteral("WriteCopy"));  //2号列为Protect属性
		if (m_OldProtect == 0)
		{
			m_OldProtect = v5.Modify.OldProtect;
		}
	}
	
}
void ProcessMemoryWindow::SetReadExecute()
{
	//获取所选行的BaseAddress和RegionSize
	QItemSelectionModel* selectionModel = ui.ProcessMemory_TableView->selectionModel();
	QModelIndex selectedRow = selectionModel->selectedRows().first();  	// 获取选中行的第一个索引
	QModelIndex baseAddressIndex = selectedRow.sibling(selectedRow.row(), 0); // 获取指定列的索引   baseaddress
	QModelIndex regionSizeIndex = selectedRow.sibling(selectedRow.row(), 1);  // RegionSize
	QString address = baseAddressIndex.data().toString();
	QString Size = regionSizeIndex.data().toString();
	bool Ok = false;
	PVOID BaseAddress = (PVOID)address.toULongLong(&Ok, 16);
	SIZE_T RegionSize = Size.toULongLong(&Ok, 16);
	//通信
	BOOL IsOk = FALSE;
	DWORD ReturnValue = 0;
	COMMUNICATE_PROCESS_MEMORY v5;
	memset(&v5, 0, sizeof(COMMUNICATE_PROCESS_MEMORY));
	v5.OperateType = MODIFY_PROCESS_MEMORY;
	v5.Modify.ProcessIdentity = (ULONG_PTR)m_ProcessId;
	v5.Modify.BaseAddress = BaseAddress;
	v5.Modify.RegionSize = RegionSize;
	v5.Modify.NewProtect = 32;  //ReadExecute

	IsOk = CommunicateDevice(&v5, sizeof(COMMUNICATE_PROCESS_MEMORY), &v5.Modify.OldProtect, sizeof(ULONG), &ReturnValue);
	if (IsOk)
	{
		ModifyProcessProtect(2, QStringLiteral("ReadExecute"));  //2号列为Protect属性
		if (m_OldProtect == 0)
		{
			m_OldProtect = v5.Modify.OldProtect;
		}
	}

}
void ProcessMemoryWindow::SetReadWriteGuard()
{
	//获取所选行的BaseAddress和RegionSize
	QItemSelectionModel* selectionModel = ui.ProcessMemory_TableView->selectionModel();
	QModelIndex selectedRow = selectionModel->selectedRows().first();  	// 获取选中行的第一个索引
	QModelIndex baseAddressIndex = selectedRow.sibling(selectedRow.row(), 0); // 获取指定列的索引   baseaddress
	QModelIndex regionSizeIndex = selectedRow.sibling(selectedRow.row(), 1);  // RegionSize
	QString address = baseAddressIndex.data().toString();
	QString Size = regionSizeIndex.data().toString();
	bool Ok = false;
	PVOID BaseAddress = (PVOID)address.toULongLong(&Ok, 16);
	SIZE_T RegionSize = Size.toULongLong(&Ok, 16);
	//通信
	BOOL IsOk = FALSE;
	DWORD ReturnValue = 0;
	COMMUNICATE_PROCESS_MEMORY v5;
	memset(&v5, 0, sizeof(COMMUNICATE_PROCESS_MEMORY));
	v5.OperateType = MODIFY_PROCESS_MEMORY;
	v5.Modify.ProcessIdentity = (ULONG_PTR)m_ProcessId;
	v5.Modify.BaseAddress = BaseAddress;
	v5.Modify.RegionSize = RegionSize;
	v5.Modify.NewProtect = 260;  //ReadWriteGuard

	IsOk = CommunicateDevice(&v5, sizeof(COMMUNICATE_PROCESS_MEMORY), &v5.Modify.OldProtect, sizeof(ULONG), &ReturnValue);
	if (IsOk)
	{
		ModifyProcessProtect(2, QStringLiteral("ReadWriteGuard"));  //2号列为Protect属性
		if (m_OldProtect == 0)
		{
			m_OldProtect = v5.Modify.OldProtect;
		}
	}

}
void ProcessMemoryWindow::RecoverProtect()
{
	//获取所选行的BaseAddress和RegionSize
	QItemSelectionModel* selectionModel = ui.ProcessMemory_TableView->selectionModel();
	QModelIndex selectedRow = selectionModel->selectedRows().first();  	// 获取选中行的第一个索引
	QModelIndex baseAddressIndex = selectedRow.sibling(selectedRow.row(), 0); // 获取指定列的索引   baseaddress
	QModelIndex regionSizeIndex = selectedRow.sibling(selectedRow.row(), 1);  // RegionSize
	QString address = baseAddressIndex.data().toString();
	QString Size = regionSizeIndex.data().toString();
	bool Ok = false;
	PVOID BaseAddress = (PVOID)address.toULongLong(&Ok, 16);
	SIZE_T RegionSize = Size.toULongLong(&Ok, 16);
	//通信
	DWORD ReturnValue = 0;
	COMMUNICATE_PROCESS_MEMORY v5;
	memset(&v5, 0, sizeof(COMMUNICATE_PROCESS_MEMORY));
	v5.OperateType = MODIFY_PROCESS_MEMORY;
	v5.Modify.ProcessIdentity = (ULONG_PTR)m_ProcessId;
	v5.Modify.BaseAddress = BaseAddress;
	v5.Modify.RegionSize = RegionSize;
	v5.Modify.NewProtect = m_OldProtect;  //Old
	const WCHAR* OldProtect = GetProtect(m_OldProtect);
	QString str = QString::fromWCharArray(OldProtect);
	ModifyProcessProtect(2, str);  //2号列为Protect属性
	CommunicateDevice(&v5, sizeof(COMMUNICATE_PROCESS_MEMORY), &v5.Modify.OldProtect, sizeof(ULONG), &ReturnValue);
}
void ProcessMemoryWindow::ModifyProcessProtect(int columnIndex, const QVariant& newValue) {
	// 获取选中模型
	QItemSelectionModel* selectionModel = ui.ProcessMemory_TableView->selectionModel();
	// 获取当前选中的行索引（只取第一个选中的行）
	QModelIndexList selectedRows = selectionModel->selectedRows();
	// 获取目标单元格的索引
	QModelIndex targetIndex = selectedRows.first().sibling(selectedRows.first().row(), columnIndex);
	if (targetIndex.isValid()) {
		ui.ProcessMemory_TableView->model()->setData(targetIndex, newValue);
	}

}
void ProcessMemoryWindow::ListProcessMemoryInfo()
{
	vector<MEMORY_INFORMATION_ENTRY> MemoryInfo;
	MemoryInfo.reserve(100);
	EnumProcessMemorys(m_ProcessId, MemoryInfo);
	vector<MEMORY_INFORMATION_ENTRY>::iterator v1;
	ULONG i = 0;
	for (v1 = MemoryInfo.begin(); v1 != MemoryInfo.end(); v1++)
	{
		QList<QStandardItem*> rowItems;
		// 地址
		rowItems.append(new QStandardItem("0x" + (QString::number((ULONG_PTR)v1->BaseAddress,16)).toUpper()));
		// 大小
		rowItems.append(new QStandardItem("0x" + (QString::number(v1->RegionSize, 16)).toUpper()));  //0x十六进制
		// Protect
		rowItems.append(new QStandardItem(QString::fromWCharArray(GetProtect(v1->Protect))));
		// State
		rowItems.append(new QStandardItem(QString::fromWCharArray(GetState(v1->State))));
		// type							 
		rowItems.append(new QStandardItem(QString::fromWCharArray(GetType(v1->Type))));
		// 将整行数据添加到模型中
		m_model.appendRow(rowItems);
	}

}

void ProcessMemoryWindow::OpenReadMemWind()
{
	ReadMemoryWindow* ReadWind = new ReadMemoryWindow(m_ProcessId);
	ReadWind->show();
}

void ProcessMemoryWindow::OpenWriteMemWind()
{
	WriteMemoryWindow* WriteWind = new WriteMemoryWindow(m_ProcessId);
	WriteWind->show();
}
