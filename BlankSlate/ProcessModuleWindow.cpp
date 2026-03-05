#include "ProcessModuleWindow.h"
#include"ProcessModule.h"
#include"ProcessHelper.h"
#include<QAbstractItemView>
ProcessModuleWindow::ProcessModuleWindow(const QString& ImageName, QWidget* parent) : QWidget(parent)
{
	ui.setupUi(this);
	// 将模型设置到视图
	ui.ProcessModule_TableView->setModel(&m_model);  //只用设置这一次，关联上之后，以后直接操作m_model就行了

	 //列表属性
	ui.ProcessModule_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // 设置选择行为为整行选中
	ui.ProcessModule_TableView->setContextMenuPolicy(Qt::CustomContextMenu); //可弹出右键菜单  必须设置
	ui.ProcessModule_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);//不可编辑
	m_model.setColumnCount(3); // 设置列数为3
	// 设置表头
	QStringList headers = { QStringLiteral("模块路径"), QStringLiteral("基地址"), QStringLiteral("大小")};
	m_model.setHorizontalHeaderLabels(headers);

	//QString->wchar_t
	std::wstring ImageNameStr = ImageName.toStdWString();
	const wchar_t* ImageNamechar = ImageNameStr.c_str();
	ListProcessModuleInfo(ImageNamechar);
	ui.ProcessModule_TableView->horizontalHeader()->resizeSection(0, 300); 


}

ProcessModuleWindow::~ProcessModuleWindow()
{

}

void ProcessModuleWindow::ListProcessModuleInfo(const wchar_t* ImageName)
{
	vector<MODULE_INFORMATION_ENTRY> ModuleInfo;
	ModuleInfo.reserve(100);
	HANDLE ProcessIdentity = GetProcessIdentity(ImageName);
	EnumProcessModules((ULONG_PTR)ProcessIdentity, ModuleInfo);
	vector<MODULE_INFORMATION_ENTRY>::iterator v1;
	ULONG i = 0;
	for (v1 = ModuleInfo.begin(); v1 != ModuleInfo.end(); v1++)
	{
		QList<QStandardItem*> rowItems;
		// ModuleFilePath
		rowItems.append(new QStandardItem(QString::fromWCharArray(v1->ModulePath)));
		// BaseAddress
		rowItems.append(new QStandardItem("0x" + (QString::number(v1->ModuleBase, 16)).toUpper()));  //0x十六进制
		// Size
		rowItems.append(new QStandardItem("0x" + (QString::number(v1->SizeOfImage, 16)).toUpper()));

		// 将整行数据添加到模型中
		m_model.appendRow(rowItems);
	}

}