#include "ProcessHandleWindow.h"
#include<QAbstractItemView>
#include"ProcessHandle.h"
#include"ProcessHelper.h"
#include<QStandardItem>
ProcessHandleWindow::ProcessHandleWindow(const QString& ImageName, QWidget *parent)	: QWidget(parent)
{
	ui.setupUi(this);
	// 将模型设置到视图
	ui.ProcessHandle_TableView->setModel(&m_model);  //只用设置这一次，关联上之后，以后直接操作m_model就行了

	 //列表属性
	ui.ProcessHandle_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // 设置选择行为为整行选中
	ui.ProcessHandle_TableView->setContextMenuPolicy(Qt::CustomContextMenu); //可弹出右键菜单  必须设置
	ui.ProcessHandle_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);//不可编辑
	m_model.setColumnCount(6); // 设置列数为3
	// 设置表头
	QStringList headers = { QStringLiteral("句柄类型"), QStringLiteral("句柄名"), QStringLiteral("句柄"),QStringLiteral("句柄对象"),QStringLiteral("句柄类型代号"),QStringLiteral("引用计数") };
	m_model.setHorizontalHeaderLabels(headers);


	std::wstring ImageNameStr = ImageName.toStdWString();
	const wchar_t* ImageNamechar = ImageNameStr.c_str();
	ListProcessHandleInfo(ImageNamechar);
	ui.ProcessHandle_TableView->horizontalHeader()->resizeSection(2, 50);
	ui.ProcessHandle_TableView->horizontalHeader()->resizeSection(3, 150); 
	ui.ProcessHandle_TableView->horizontalHeader()->resizeSection(4, 90);
	ui.ProcessHandle_TableView->horizontalHeader()->resizeSection(5, 70);

}

ProcessHandleWindow::~ProcessHandleWindow()
{}
void ProcessHandleWindow::ListProcessHandleInfo(const wchar_t* ImageName)
{
	vector<HANDLE_INFORMATION_ENTRY> HandleInfo;
	HandleInfo.reserve(1000);
	int i = 0;
	HANDLE ProcessIdentity = GetProcessIdentity(ImageName);
	EnumProcessHandles(ProcessIdentity, HandleInfo);
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
		//引用计数
		rowItems.append(new QStandardItem(QString::number(v1->Count)));
		// 将整行数据添加到模型中
		m_model.appendRow(rowItems);
	}
}