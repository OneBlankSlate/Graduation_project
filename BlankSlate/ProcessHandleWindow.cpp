#include "ProcessHandleWindow.h"
#include<QAbstractItemView>
#include"ProcessHandle.h"
#include"ProcessHelper.h"
#include<QStandardItem>
#include<QMessageBox>
#include<QDateTime>
#include<QFile>
#include<QTextStream>
#include<QStandardPaths>
// 方案二：单个构造函数
ProcessHandleWindow::ProcessHandleWindow(HANDLE ProcessId, QWidget* parent)	: QWidget(parent), m_ProcessId(ProcessId)  // 保存进程句柄/ID
{
	ui.setupUi(this);
	// 将模型设置到视图
	ui.ProcessHandle_TableView->setModel(&m_model);  //只用设置这一次，关联上之后，以后直接操作m_model就行了

	 //列表属性
	ui.ProcessHandle_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // 设置选择行为为整行选中
	ui.ProcessHandle_TableView->setContextMenuPolicy(Qt::CustomContextMenu); //可弹出右键菜单  必须设置
	ui.ProcessHandle_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);//不可编辑
	m_model.setColumnCount(5); // 设置列数为5
	// 设置表头
	QStringList headers = { QStringLiteral("句柄类型"), QStringLiteral("句柄名"), QStringLiteral("句柄"),QStringLiteral("句柄对象"),QStringLiteral("句柄类型代号")};
	m_model.setHorizontalHeaderLabels(headers);


	/*std::wstring ImageNameStr = ImageName.toStdWString();
	const wchar_t* ImageNamechar = ImageNameStr.c_str();*/
	ListProcessHandleInfo(ProcessId);
	ui.ProcessHandle_TableView->horizontalHeader()->resizeSection(0, 100);
	ui.ProcessHandle_TableView->horizontalHeader()->resizeSection(1, 200);
	ui.ProcessHandle_TableView->horizontalHeader()->resizeSection(2, 80);
	ui.ProcessHandle_TableView->horizontalHeader()->resizeSection(3, 150);
	ui.ProcessHandle_TableView->horizontalHeader()->resizeSection(4, 100);

	m_TableViewMenu = new QMenu(ui.ProcessHandle_TableView);
	CloseHandleAct = new QAction(QStringLiteral("关闭句柄"), ui.ProcessHandle_TableView);
	ExportAct = new QAction(QStringLiteral("导出"), ui.ProcessHandle_TableView);
	m_TableViewMenu->addAction(CloseHandleAct);
	m_TableViewMenu->addAction(ExportAct);
	//消息关联
	connect(ui.ProcessHandle_TableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(Menu_Slot(QPoint)));  //菜单初始化
	connect(CloseHandleAct, &QAction::triggered, this, &ProcessHandleWindow::CloseHandle);
	connect(ExportAct, &QAction::triggered, this, &ProcessHandleWindow::ExportSelected);
}

ProcessHandleWindow::~ProcessHandleWindow()
{}
void ProcessHandleWindow::ListProcessHandleInfo(HANDLE ProcessId)
{
	vector<HANDLE_INFORMATION_ENTRY> HandleInfo;
	HandleInfo.reserve(1000);
	int i = 0;
	EnumProcessHandles(ProcessId, HandleInfo);
	_tprintf(_T("ViewSize:%d\r\n"), HandleInfo.size());
	vector<HANDLE_INFORMATION_ENTRY>::iterator v1;
	for (v1 = HandleInfo.begin(); v1 != HandleInfo.end(); v1++)
	{
		QList<QStandardItem*> rowItems;
		// 句柄类型
		rowItems.append(new QStandardItem(QString::fromWCharArray(v1->HandleType)));
		// 句柄名
		rowItems.append(new QStandardItem(QString::fromWCharArray(v1->HandleName)));  //0x十六进制
		// 句柄
		rowItems.append(new QStandardItem("0x" + (QString::number((ULONG_PTR)v1->Handle, 16)).toUpper()));
		// 句柄对象
		rowItems.append(new QStandardItem("0x" + (QString::number((ULONG_PTR)v1->Object,16)).toUpper()));
		// 句柄类型代号
		rowItems.append(new QStandardItem(QString::number(v1->Index)));
		// 将整行数据添加到模型中
		m_model.appendRow(rowItems);
	}
}
void ProcessHandleWindow::CloseHandle()
{
	EnableDebugPrivilege();
	QModelIndexList selectedRows = ui.ProcessHandle_TableView->selectionModel()->selectedRows();
	if (!selectedRows.isEmpty()) {
		QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
		QModelIndex targetIndex = index.sibling(index.row(), 2); // 获取第 2 列的索引-句柄值
		QString value = targetIndex.data().toString(); // 获取该列的值
		 // 转换为十六进制数值
		bool ok;
		HANDLE targetHandle = (HANDLE)value.toULongLong(&ok, 16);
		BOOL IsOk = FALSE;
		COMMUNICATE_CLOSE_HANDLE v1;
		v1.OperateType = CLOSE_HANDLE;
		v1.ProcessId = m_ProcessId;
		v1.TargetHandle = targetHandle;
		do
		{
			IsOk = CommunicateDevice(&v1, sizeof(COMMUNICATE_CLOSE_HANDLE), NULL, 0, NULL);
		} while (!IsOk);
		if (IsOk) {
			m_model.removeRow(index.row());
		}
	}
	
}

void ProcessHandleWindow::Menu_Slot(QPoint p)
{
	QModelIndex index = ui.ProcessHandle_TableView->indexAt(p);//获取鼠标点击位置项的索引
	if (index.isValid())//数据项是否有效，空白处点击无菜单
	{
		QItemSelectionModel* selections = ui.ProcessHandle_TableView->selectionModel();//获取当前的选择模型
		QModelIndexList selected = selections->selectedIndexes();//返回当前选择的模型索引
		if (selected.count() == 1) //选择单个项目时的右键菜单显示Action1
		{
			//RefreshAct->setVisible(true);
			m_TableViewMenu->setVisible(true);
		}
		else   //如果选中多个项目，则右键菜单显示Action2
		{
			//ModuleAct->setVisible(true);
			m_TableViewMenu->setVisible(true);
		}
		m_TableViewMenu->exec(QCursor::pos());//数据项有效才显示菜单
	}

}

void ProcessHandleWindow::ExportSelected()
{
	QModelIndexList selectedRows = ui.ProcessHandle_TableView->selectionModel()->selectedRows();
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
	QString filePath = desktopPath + "/ProcessHandle.txt";

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