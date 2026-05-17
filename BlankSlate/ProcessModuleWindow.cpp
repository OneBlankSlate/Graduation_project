#include "ProcessModuleWindow.h"
#include "ProcessModule.h"
#include "ProcessHelper.h"
#include <QAbstractItemView>
#include <QMessageBox>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>

ProcessModuleWindow::ProcessModuleWindow(DWORD ProcessId, const QString& ImageName, QWidget* parent)
	: QWidget(parent), m_processId(ProcessId), m_imageName(ImageName)
{
	ui.setupUi(this);
	// 将模型设置到视图
	ui.ProcessModule_TableView->setModel(&m_model);

	// 列表属性
	ui.ProcessModule_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui.ProcessModule_TableView->setContextMenuPolicy(Qt::CustomContextMenu);
	ui.ProcessModule_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_model.setColumnCount(3);
	QStringList headers = { QStringLiteral("模块路径"), QStringLiteral("基地址"), QStringLiteral("大小") };
	m_model.setHorizontalHeaderLabels(headers);

	// 右键菜单
	m_TableViewMenu = new QMenu(ui.ProcessModule_TableView);
	RefreshAct = new QAction(QStringLiteral("刷新"), ui.ProcessModule_TableView);
	UnloadAct = new QAction(QStringLiteral("卸载模块"), ui.ProcessModule_TableView);
	m_TableViewMenu->addAction(RefreshAct);
	m_TableViewMenu->addAction(UnloadAct);
	ExportAct = new QAction(QStringLiteral("导出"), ui.ProcessModule_TableView);
	m_TableViewMenu->addAction(ExportAct);

	// 信号连接
	connect(ui.ProcessModule_TableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(Menu_Slot(QPoint)));
	connect(RefreshAct, &QAction::triggered, this, &ProcessModuleWindow::RefreshModules);
	connect(UnloadAct, &QAction::triggered, this, &ProcessModuleWindow::UnloadModule);
	connect(ExportAct, &QAction::triggered, this, &ProcessModuleWindow::ExportSelected);

	// 加载模块列表
	ListProcessModuleInfoByPid();
	ui.ProcessModule_TableView->horizontalHeader()->resizeSection(0, 300);

	// 设置窗口标题包含进程名
	setWindowTitle(QStringLiteral("进程模块 - %1 (PID: %2)").arg(ImageName).arg(ProcessId));
}

ProcessModuleWindow::~ProcessModuleWindow()
{
}

void ProcessModuleWindow::ListProcessModuleInfoByPid()
{
	vector<MODULE_INFORMATION_ENTRY> ModuleInfo;
	ModuleInfo.reserve(100);
	EnumProcessModules((ULONG_PTR)m_processId, ModuleInfo);
	vector<MODULE_INFORMATION_ENTRY>::iterator v1;
	for (v1 = ModuleInfo.begin(); v1 != ModuleInfo.end(); v1++)
	{
		QList<QStandardItem*> rowItems;
		// ModuleFilePath
		rowItems.append(new QStandardItem(QString::fromWCharArray(v1->ModulePath)));
		// BaseAddress
		rowItems.append(new QStandardItem("0x" + (QString::number(v1->ModuleBase, 16)).toUpper()));
		// Size
		rowItems.append(new QStandardItem("0x" + (QString::number(v1->SizeOfImage, 16)).toUpper()));

		m_model.appendRow(rowItems);
	}
}

void ProcessModuleWindow::Menu_Slot(QPoint p)
{
	QModelIndex index = ui.ProcessModule_TableView->indexAt(p);
	if (index.isValid())
	{
		m_TableViewMenu->exec(QCursor::pos());
	}
}

void ProcessModuleWindow::RefreshModules()
{
	m_model.removeRows(0, m_model.rowCount());
	ListProcessModuleInfoByPid();
}

void ProcessModuleWindow::UnloadModule()
{
	QModelIndexList selectedRows = ui.ProcessModule_TableView->selectionModel()->selectedRows();
	if (selectedRows.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择要卸载的模块！"));
		return;
	}

	// 获取选中行的模块路径和基地址
	QModelIndex index = selectedRows.first();
	QString modulePath = index.sibling(index.row(), 0).data().toString();
	QString baseAddrStr = index.sibling(index.row(), 1).data().toString();

	// 确认卸载
	QMessageBox::StandardButton reply = QMessageBox::warning(
		this,
		QStringLiteral("确认卸载"),
		QStringLiteral("确定要卸载模块吗？\n\n模块路径: %1\n基地址: %2\n\n卸载模块可能导致目标进程不稳定！").arg(modulePath).arg(baseAddrStr),
		QMessageBox::Yes | QMessageBox::No
	);
	if (reply != QMessageBox::Yes) return;

	// 解析基地址
	QString hexStr = baseAddrStr;
	hexStr.remove(0, 2); // 去掉 "0x" 前缀
	bool ok = false;
	ULONG_PTR moduleBase = hexStr.toULongLong(&ok, 16);
	if (!ok)
	{
		QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("无法解析模块基地址！"));
		return;
	}

	// 通过 CreateRemoteThread 在目标进程中调用 FreeLibrary 卸载模块
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_processId);
	if (hProcess == NULL)
	{
		QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("无法打开目标进程！错误码: %1").arg(GetLastError()));
		return;
	}

	HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
	if (hKernel32 == NULL)
	{
		CloseHandle(hProcess);
		QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("无法获取kernel32.dll模块句柄！"));
		return;
	}

	FARPROC pFreeLibrary = GetProcAddress(hKernel32, "FreeLibrary");
	if (pFreeLibrary == NULL)
	{
		CloseHandle(hProcess);
		QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("无法获取FreeLibrary地址！"));
		return;
	}

	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
		(LPTHREAD_START_ROUTINE)pFreeLibrary, (LPVOID)moduleBase, 0, NULL);
	if (hThread == NULL)
	{
		CloseHandle(hProcess);
		QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("创建远程线程失败！错误码: %1").arg(GetLastError()));
		return;
	}

	// 等待远程线程执行完毕
	WaitForSingleObject(hThread, INFINITE);

	// 检查 FreeLibrary 返回值：非0表示成功，0表示失败
	DWORD exitCode = 0;
	GetExitCodeThread(hThread, &exitCode);
	CloseHandle(hThread);
	CloseHandle(hProcess);

	if (exitCode == 0)
	{
		QMessageBox::critical(this, QStringLiteral("错误"),
			QStringLiteral("FreeLibrary 执行失败！模块可能仍在使用中，无法卸载。\n\n模块: %1").arg(modulePath));
	}
	else
	{
		QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("模块卸载成功！"));
		// 刷新模块列表
		RefreshModules();
	}
}

void ProcessModuleWindow::ExportSelected()
{
	QModelIndexList selectedRows = ui.ProcessModule_TableView->selectionModel()->selectedRows();
	QList<int> rowsToExport;

	if (!selectedRows.isEmpty()) {
		for (const auto& index : selectedRows) {
			if (!rowsToExport.contains(index.row())) {
				rowsToExport.append(index.row());
			}
		}
	} else {
		for (int i = 0; i < m_model.rowCount(); i++) {
			rowsToExport.append(i);
		}
	}

	if (rowsToExport.isEmpty()) {
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("没有可导出的数据！"));
		return;
	}

	QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
	QString filePath = desktopPath + "/ProcessModule.txt";

	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("无法创建文件: %1").arg(filePath));
		return;
	}

	QTextStream out(&file);
	out.setCodec("UTF-8");
	out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n\n";

	for (int col = 0; col < m_model.columnCount(); col++) {
		if (col > 0) out << "\t";
		out << m_model.horizontalHeaderItem(col)->text();
	}
	out << "\n";

	for (int row : rowsToExport) {
		for (int col = 0; col < m_model.columnCount(); col++) {
			if (col > 0) out << "\t";
			out << m_model.item(row, col)->text();
		}
		out << "\n";
	}

	file.close();
	QMessageBox::information(this, QStringLiteral("成功"),
		QStringLiteral("已导出 %1 条记录到 %2").arg(rowsToExport.size()).arg(filePath));
}
