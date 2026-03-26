#include "ProcessWindow.h"
#include<QTableView>
#include<QHeaderView>
#include"ProcessHelper.h"
#include"ProcessModuleWindow.h"
#include"ProcessHandleWindow.h"
#include"ProcessMemoryWindow.h"
#include"DriverModule.h"
ProcessWindow::ProcessWindow(QWidget *parent) : QWidget(parent)
{
	ui.setupUi(this);
    //列表属性
    ui.Process_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // 设置选择行为为整行选中
    ui.Process_TableView->horizontalHeader()->setStretchLastSection(true); //最后一列填满表
    ui.Process_TableView->setContextMenuPolicy(Qt::CustomContextMenu); //可弹出右键菜单  必须设置
    ui.Process_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);//不可编辑
    //m_model = new QStandardItemModel();  //指针类型
    

    m_model.setColumnCount(5); // 设置列数为5
    // 设置表头
    QStringList headers = { QStringLiteral("映像名称"), QStringLiteral("进程ID"), QStringLiteral("父进程ID"), QStringLiteral("映像路径"), QStringLiteral("EPROCESS") };
    m_model.setHorizontalHeaderLabels(headers);

    // 将模型设置到视图
    ui.Process_TableView->setModel(&m_model);  //只用设置这一次，关联上之后，以后直接操作m_model就行了

    ui.Process_TableView->show();
    //获取进程信息并打印
    ListProcessInfo();
    // 设置每一列的宽度  宽度的设置必须在信息插入完成后进行，否则会被覆盖！
    ui.Process_TableView->horizontalHeader()->resizeSection(0, 100); // 第一列宽度为100
    ui.Process_TableView->horizontalHeader()->resizeSection(1, 50); // 第二列宽度为50
    ui.Process_TableView->horizontalHeader()->resizeSection(2, 60); // 第三列宽度为60
    ui.Process_TableView->horizontalHeader()->resizeSection(3, 400); // 第三列宽度为200
    ui.Process_TableView->horizontalHeader()->resizeSection(4, 150); // 第三列宽度为80
    //添加菜单项
    m_TableViewMenu = new QMenu(ui.Process_TableView);
    RefreshAct = new QAction(QStringLiteral("刷新"), ui.Process_TableView);
    ModuleAct = new QAction(QStringLiteral("查看进程模块"), ui.Process_TableView);
    HandleAct = new QAction(QStringLiteral("查看进程句柄"), ui.Process_TableView);
    MemoryAct = new QAction(QStringLiteral("查看进程内存"), ui.Process_TableView);
    TerminateProcessAct = new QAction(QStringLiteral("结束进程"), ui.Process_TableView);
    HideProcessAct = new QAction(QStringLiteral("隐藏进程"), ui.Process_TableView);
    ProtectProcessAct = new QAction(QStringLiteral("保护进程"), ui.Process_TableView);
    UnprotectProcessAct = new QAction(QStringLiteral("撤销保护"), ui.Process_TableView);
    Hook_NtTerminateProAct = new QAction(QStringLiteral("hook类型-进程防关闭"),ui.Process_TableView);
    Unhook_NtTerminateProAct = new QAction(QStringLiteral("unhook类型-进程防关闭"), ui.Process_TableView);
    Hook_NtWriteVirtualMemoryAct = new QAction(QStringLiteral("hook类型-进程防写入"), ui.Process_TableView);
    Unhook_NtWriteVirtualMemoryAct = new QAction(QStringLiteral("unhook类型-进程防写入"), ui.Process_TableView);
    m_TableViewMenu->addAction(RefreshAct);
    m_TableViewMenu->addAction(ModuleAct);
    m_TableViewMenu->addAction(HandleAct);
    m_TableViewMenu->addAction(MemoryAct);
    m_TableViewMenu->addAction(TerminateProcessAct);
    m_TableViewMenu->addAction(HideProcessAct);
    m_TableViewMenu->addAction(ProtectProcessAct);
    m_TableViewMenu->addAction(UnprotectProcessAct);
    m_TableViewMenu->addAction(Hook_NtTerminateProAct);
    m_TableViewMenu->addAction(Unhook_NtTerminateProAct);
    m_TableViewMenu->addAction(Hook_NtWriteVirtualMemoryAct);
    m_TableViewMenu->addAction(Unhook_NtWriteVirtualMemoryAct);

    //消息关联
    connect(ui.Process_TableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(Menu_Slot(QPoint)));  //菜单初始化
    connect(RefreshAct, &QAction::triggered, this, &ProcessWindow::RefreshProcess);
    connect(ModuleAct, &QAction::triggered,this, &ProcessWindow::OpenProcessModuleWindow);
    connect(HandleAct, &QAction::triggered, this, &ProcessWindow::OpenProcessHandleWindow);
    connect(MemoryAct, &QAction::triggered, this, &ProcessWindow::OpenProcessMemoryWindow);
    connect(TerminateProcessAct, &QAction::triggered, this, &ProcessWindow::TerminateProcess);
    connect(HideProcessAct, &QAction::triggered, this, &ProcessWindow::HideProcess);
    connect(ProtectProcessAct, &QAction::triggered, this, &ProcessWindow::ProtectProcess);
    connect(UnprotectProcessAct, &QAction::triggered, this, &ProcessWindow::UnprotectProcess);
    connect(Hook_NtTerminateProAct, &QAction::triggered, this, &ProcessWindow::hook_NtTerminateProcess);
    connect(Unhook_NtTerminateProAct, &QAction::triggered, this, &ProcessWindow::unhook_NtTerminateProcess);
    connect(Hook_NtWriteVirtualMemoryAct, &QAction::triggered, this, &ProcessWindow::hook_NtWriteVirtualMemory);
    connect(Unhook_NtWriteVirtualMemoryAct, &QAction::triggered, this, &ProcessWindow::unhook_NtWriteVirtualMemory);
    
}

ProcessWindow::~ProcessWindow()
{


}

void ProcessWindow::ListProcessInfo()
{
    vector<PROCESS_INFORMATION_ENTRY> ProcessInfo;
    ProcessInfo.reserve(100);
    EnumProcess(ProcessInfo);
    // 添加数据
    vector<PROCESS_INFORMATION_ENTRY>::iterator v1;
    for (v1 = ProcessInfo.begin(); v1 != ProcessInfo.end(); v1++)
    {
        QList<QStandardItem*> rowItems;
        // ImageFileName
        rowItems.append(new QStandardItem(QString::fromUtf8(v1->ImageName)));
        // ProcessIdentity
        rowItems.append(new QStandardItem(QString::number(v1->ProcessIdentity)));
        // ParentPid
        rowItems.append(new QStandardItem(QString::number(v1->ParentPid)));
        // ProcessPath
        rowItems.append(new QStandardItem(QString::fromWCharArray(v1->ProcessPath)));
        // EProcess
        rowItems.append(new QStandardItem("0x" + (QString::number((ULONG_PTR)v1->EProcess, 16)).toUpper()));

        // 将整行数据添加到模型中
        m_model.appendRow(rowItems);
    }
}

void ProcessWindow::TerminateProcess()
{
    QModelIndexList selectedRows = ui.Process_TableView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
        QModelIndex targetIndex = index.sibling(index.row(), 1); // 获取第 1 列的索引-id
        QString value = targetIndex.data().toString(); // 获取该列的值

        BOOL IsOk = FALSE;
        COMMUNICATE_TERMINATE_PROCESS v1;
        v1.OperateType = TERMINATE_PROCESS;
        v1.ProcessIdentity = (HANDLE)value.toULongLong();
        do
        {
            IsOk = CommunicateDevice(&v1, sizeof(COMMUNICATE_TERMINATE_PROCESS), NULL, 0, NULL);
        } while (!IsOk);
    }
}

void ProcessWindow::HideProcess()
{
    QModelIndexList selectedRows = ui.Process_TableView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
        QModelIndex targetIndex = index.sibling(index.row(), 1); // 获取第 1 列的索引-id
        QString value = targetIndex.data().toString(); // 获取该列的值

        BOOL IsOk = FALSE;
        COMMUNICATE_HIDE_PROCESS v1;
        v1.OperateType = HIDE_PROCESS;
        v1.ProcessIdentity = (HANDLE)value.toULongLong();
        do
        {
            IsOk = CommunicateDevice(&v1, sizeof(COMMUNICATE_HIDE_PROCESS), NULL, 0, NULL);
        } while (!IsOk);
    }
    
}

void ProcessWindow::ProtectProcess()
{
    QModelIndexList selectedRows = ui.Process_TableView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
        QModelIndex targetIndex = index.sibling(index.row(), 1); // 获取第 1 列的索引-id
        QString value = targetIndex.data().toString(); // 获取该列的值

        BOOL IsOk = FALSE;
        COMMUNICATE_PROTECT_PROCESS v1;
        RtlZeroMemory(&v1, sizeof(COMMUNICATE_PROTECT_PROCESS));
        v1.OperateType = PROTECT_PROCESS;
        v1.ProcessIdentitys[v1.NumberOfProcess++] = (HANDLE)value.toULongLong();
        do
        {
            IsOk = CommunicateDevice(&v1, sizeof(COMMUNICATE_PROTECT_PROCESS), NULL, 0, NULL);
        } while (!IsOk);
    }
}

void ProcessWindow::UnprotectProcess()
{
    QModelIndexList selectedRows = ui.Process_TableView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
        QModelIndex targetIndex = index.sibling(index.row(), 1); // 获取第 1 列的索引-id
        QString value = targetIndex.data().toString(); // 获取该列的值

        BOOL IsOk = FALSE;
        COMMUNICATE_PROTECT_PROCESS v1;
        RtlZeroMemory(&v1, sizeof(COMMUNICATE_PROTECT_PROCESS));
        v1.OperateType = UNPROTECT_PROCESS;
        v1.ProcessIdentitys[v1.NumberOfProcess++] = (HANDLE)value.toULongLong();
        do
        {
            IsOk = CommunicateDevice(&v1, sizeof(COMMUNICATE_PROTECT_PROCESS), NULL, 0, NULL);
        } while (!IsOk);
    }
}

void ProcessWindow::hook_NtTerminateProcess()
{
    /*const char* driverPath = "C:\\Users\\24846\\Desktop\\qt_exe2\\etw_hook.sys";

    if (LoadDriver(driverPath)) {
        HANDLE hDevice = CreateFile(
            L"\\\\.\\EtwHook",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hDevice == INVALID_HANDLE_VALUE) {
            MessageBox(NULL, _T("打开设备失败"), _T("提示"), NULL);
        }*/



        //HANDLE hDevice = CreateFile(
        //    L"\\\\.\\EtwHook",
        //    GENERIC_READ | GENERIC_WRITE,
        //    0,
        //    NULL,
        //    OPEN_EXISTING,
        //    FILE_ATTRIBUTE_NORMAL,
        //    NULL
        //);
        //if (hDevice == INVALID_HANDLE_VALUE) {
        //    MessageBox(NULL, _T("打开设备失败"), _T("提示"), NULL);
        //}
        //PROTECT_CONFIG config = { 0 };
        //QModelIndexList selectedRows = ui.Process_TableView->selectionModel()->selectedRows();
        //if (!selectedRows.isEmpty()) {
        //    QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
        //    QModelIndex targetIndex = index.sibling(index.row(), 0); // 获取第 0 列的索引-进程名
        //    QString value = targetIndex.data().toString(); // 获取该列的值
        //    // 将QString转换为std::wstring
        //    std::wstring wstr = value.toStdWString();

        //    // 检查长度，避免溢出
        //    if (wstr.length() < 256) {
        //        wcscpy_s(config.ProcessName, 256, wstr.c_str()); // 使用安全复制函数
        //    }
        //    else {
        //        // 处理溢出情况，例如截断字符串
        //        wcsncpy_s(config.ProcessName, 256, wstr.c_str(), 255);
        //        config.ProcessName[255] = L'\0'; // 确保空终止
        //    }

        //}
        //config.IsProtected = TRUE;

        //DWORD bytesReturned = 0;
        //BOOL result = DeviceIoControl(
        //    hDevice,
        //    IOCTL_PROTECT_TERMINATE,
        //    &config,
        //    sizeof(config),
        //    NULL,
        //    0,
        //    &bytesReturned,
        //    NULL
        //);
    
   
}

void ProcessWindow::unhook_NtTerminateProcess()
{
   /* __debugbreak();
    const char* driverPath = "C:\\Users\\24846\\Desktop\\qt_exe2\\etw_hook.sys";

    char serviceName[MAX_PATH];
    const char* pFileName = strrchr(driverPath, '\\');
    if (pFileName == NULL) pFileName = driverPath;
    else pFileName++;

    strncpy(serviceName, pFileName, MAX_PATH);
    char* pExt = strstr(serviceName, ".sys");
    if (pExt != NULL) *pExt = '\0';
    if (UnloadDriver(serviceName))
    {

    }
    else {
        ::MessageBox(NULL, _T("驱动卸载失败"), _T("提示"), NULL);
    }*/

}

void ProcessWindow::hook_NtWriteVirtualMemory()
{
}

void ProcessWindow::unhook_NtWriteVirtualMemory()
{
}


void ProcessWindow::OpenProcessModuleWindow()
{
    QModelIndexList selectedRows = ui.Process_TableView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
        QModelIndex targetIndex = index.sibling(index.row(), 0); // 获取第 0 列的索引
        QString value = targetIndex.data().toString(); // 获取该列的值
        // 得到了目标进程名   作为参数传递给模块窗口
        ProcessModuleWindow* ProcessModuleWind = new ProcessModuleWindow(value);
        ProcessModuleWind->show();
    }
}

void ProcessWindow::OpenProcessHandleWindow()
{
    QModelIndexList selectedRows = ui.Process_TableView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
        QModelIndex targetIndex = index.sibling(index.row(), 0); // 获取第 0 列的索引
        QString value = targetIndex.data().toString(); // 获取该列的值
        // 得到了目标进程名   作为参数传递给模块窗口
        ProcessHandleWindow* ProcessModuleWind = new ProcessHandleWindow(value);
        ProcessModuleWind->show();
    }
}

void ProcessWindow::OpenProcessMemoryWindow()
{
    
    QModelIndexList selectedRows = ui.Process_TableView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        QModelIndex index = selectedRows.first(); // 获取选中行的第一个索引
        QModelIndex targetIndex = index.sibling(index.row(), 1); // 获取第 1 列的索引-id
        QString value = targetIndex.data().toString(); // 获取该列的值
        // 得到了目标进程id   作为参数传递给模块窗口
        ProcessMemoryWindow* ProcessModuleWind = new ProcessMemoryWindow(value);
        ProcessModuleWind->show();
    }
}

void ProcessWindow::Menu_Slot(QPoint p)
{
    QModelIndex index = ui.Process_TableView->indexAt(p);//获取鼠标点击位置项的索引
    if (index.isValid())//数据项是否有效，空白处点击无菜单
    {
        QItemSelectionModel* selections = ui.Process_TableView->selectionModel();//获取当前的选择模型
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

void ProcessWindow::RefreshProcess()
{
    m_model.clear();
    ListProcessInfo();
}
