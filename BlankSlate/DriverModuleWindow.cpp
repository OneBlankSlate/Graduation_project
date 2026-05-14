#include "DriverModuleWindow.h"
#include"SystemModule.h"
#include"DriverModule.h"
#include<vector>
DriverModuleWindow::DriverModuleWindow(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
    m_model.setColumnCount(5); // 设置列数为7
    //列表属性
    ui.DriverModule_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // 设置选择行为为整行选中
    ui.DriverModule_TableView->setContextMenuPolicy(Qt::CustomContextMenu); //可弹出右键菜单  必须设置
    ui.DriverModule_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);//不可编辑
    // 将模型设置到视图
    ui.DriverModule_TableView->setModel(&m_model);
    // 设置表头
    QStringList headers = { QStringLiteral("驱动名"), QStringLiteral("基地址"), QStringLiteral("大小"), QStringLiteral("驱动模块入口"), QStringLiteral("驱动路径")};
    m_model.setHorizontalHeaderLabels(headers);
    
    // 添加数据
    ListDriverModules();

    //添加菜单项
    m_TableViewMenu = new QMenu(ui.DriverModule_TableView);
    RefreshAct = new QAction(QStringLiteral("刷新"), ui.DriverModule_TableView);
    UnloadAct= new QAction(QStringLiteral("卸载"), ui.DriverModule_TableView);
    m_TableViewMenu->addAction(RefreshAct);
    m_TableViewMenu->addAction(UnloadAct);
    //消息关联
    connect(ui.DriverModule_TableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(Menu_Slot(QPoint)));  //菜单初始化
    connect(RefreshAct, &QAction::triggered, this, &DriverModuleWindow::RefreshDriverModule);
    connect(UnloadAct, &QAction::triggered, this, &DriverModuleWindow::UnloadDriverModule);
    
}

DriverModuleWindow::~DriverModuleWindow()
{}

void DriverModuleWindow::ListDriverModules()
{
    //__debugbreak();
    vector<DRIVER_MODULE_ENTRY> DriverModuleInfo;
    DriverModuleInfo.reserve(100);
    EnumDriverModule(DriverModuleInfo);
    // 添加数据
    vector<DRIVER_MODULE_ENTRY>::iterator v1;
    for (v1 = DriverModuleInfo.begin(); v1 != DriverModuleInfo.end(); v1++)
    {
        QList<QStandardItem*> rowItems;
        // DriverName
        rowItems.append(new QStandardItem(QString::fromWCharArray(v1->DriverName)));
        // ModuleBase
        rowItems.append(new QStandardItem("0x"+(QString::number(v1->ModuleBase, 16)).toUpper()));
        // ModuleSize
        rowItems.append(new QStandardItem("0x"+(QString::number(v1->SizeOfImage, 16)).toUpper()));
        // EntryPoint
        rowItems.append(new QStandardItem("0x"+(QString::number(v1->EntryPoint, 16)).toUpper()));
        // DriverPath
        rowItems.append(new QStandardItem(QString::fromWCharArray(v1->DriverPath)));

        // 将整行数据添加到模型中
        m_model.appendRow(rowItems);
    }


}

void DriverModuleWindow::RefreshDriverModule()
{
    //m_model.clear();  //这种方法清理数据行的同时也会清理表头，因此每次刷新之后表头都会变成数字
    //只清理数据行，不清理表头
    m_model.removeRows(0, m_model.rowCount());
    ListDriverModules();
}

void DriverModuleWindow::UnloadDriverModule()
{
    QModelIndexList selectedRows = ui.DriverModule_TableView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
        QModelIndex targetIndex = index.sibling(index.row(), 0); 
        QString value = targetIndex.data().toString(); // 获取该列的值
         // 续写部分：获取纯驱动名（去掉.sys后缀）
        QString driverName = value;

        // 移除.sys后缀
        if (driverName.endsWith(".sys", Qt::CaseInsensitive)) {
            driverName = driverName.left(driverName.length() - 4); // 去掉".sys"（4个字符）
        }

        // 转换为char*类型
        QByteArray byteArray = driverName.toLocal8Bit();
        char* driverNameChar = byteArray.data();
        UnloadDriver(driverNameChar);
    }
}

void DriverModuleWindow::Menu_Slot(QPoint p)
{
    QModelIndex index = ui.DriverModule_TableView->indexAt(p);//获取鼠标点击位置项的索引
    if (index.isValid())//数据项是否有效，空白处点击无菜单
    {
        QItemSelectionModel* selections = ui.DriverModule_TableView->selectionModel();//获取当前的选择模型
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
