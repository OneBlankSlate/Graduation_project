// ThreadMonWindow.cpp
#include "ThreadMonWindow.h"
#include "ui_ThreadMonWindow.h"
#include <QDateTime>
#include <QMessageBox>
#include <QHeaderView>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include"ThreadMonCommon.h"
#include"ThreadMonitor.h"
#include <psapi.h>
ThreadMonWindow::ThreadMonWindow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ThreadMonWindowClass)
    , m_updateTimer(new QTimer(this))
    , m_statusTimer(new QTimer(this))
    , m_totalEvents(0)
    , m_currentEvents(0)
{
    ui->setupUi(this);

    // 初始化模型
    m_model = new QStandardItemModel(this);
    setupTableView();

    // 连接按钮信号
    connect(ui->ThreadMonStartBtn, &QPushButton::clicked, this, &ThreadMonWindow::onStartClicked);
    connect(ui->ThreadMonStopBtn, &QPushButton::clicked, this, &ThreadMonWindow::onStopClicked);
    connect(ui->ThreadMonRefreshBtn, &QPushButton::clicked, this, &ThreadMonWindow::onRefreshClicked);
    connect(ui->ThreadMonClearBtn, &QPushButton::clicked, this, &ThreadMonWindow::onClearClicked);

    // 导出按钮
    btnExport = new QPushButton(QStringLiteral("导出"));
    ui->horizontalLayout_2->insertWidget(4, btnExport);
    connect(btnExport, &QPushButton::clicked, this, &ThreadMonWindow::onExportClicked);

    // 设置定时器
    m_updateTimer->setInterval(1000); // 1秒更新一次事件
    connect(m_updateTimer, &QTimer::timeout, this, &ThreadMonWindow::updateEvents);

    // 状态更新定时器
    m_statusTimer->setInterval(5000); // 5秒更新一次状态
    connect(m_statusTimer, &QTimer::timeout, this, &ThreadMonWindow::updateStatus);

    // 初始状态
    updateUIState(false);

}

ThreadMonWindow::~ThreadMonWindow()
{
    // 停止监控
    if (ui->ThreadMonStopBtn->isEnabled()) {
        onStopClicked();
    }

    // m_model 以 this 为父对象创建，Qt 对象树会自动删除，无需手动释放

    delete ui;
}

void ThreadMonWindow::setupTableView()
{
    // 设置表头
    QStringList headers = {
        QStringLiteral("事件类型"),
        QStringLiteral("时间"),
        QStringLiteral("线程ID"),
        QStringLiteral("进程ID"),
        QStringLiteral("进程名"),
        QStringLiteral("映像路径"),
        QStringLiteral("模块名"),
        QStringLiteral("优先级"),
        QStringLiteral("基本优先级"),
        QStringLiteral("退出状态")
    };

    m_model->setHorizontalHeaderLabels(headers);

    // 设置表格属性
    ui->ThreadMon_TableView->setModel(m_model);
    ui->ThreadMon_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->ThreadMon_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->ThreadMon_TableView->horizontalHeader()->setStretchLastSection(true);
    ui->ThreadMon_TableView->setSortingEnabled(true);

    // 设置列宽
    ui->ThreadMon_TableView->setColumnWidth(0, 60);    // 事件类型
    ui->ThreadMon_TableView->setColumnWidth(1, 200);   // 时间
    ui->ThreadMon_TableView->setColumnWidth(2, 60);    // 线程ID
    ui->ThreadMon_TableView->setColumnWidth(3, 60);    // 进程ID
    ui->ThreadMon_TableView->setColumnWidth(4, 200);   // 进程名
    ui->ThreadMon_TableView->setColumnWidth(5, 600);   // 映像路径
    ui->ThreadMon_TableView->setColumnWidth(6, 100);   // 模块名
    ui->ThreadMon_TableView->setColumnWidth(7, 55);    // 优先级
    ui->ThreadMon_TableView->setColumnWidth(8, 70);    // 基本优先级
    ui->ThreadMon_TableView->setColumnWidth(9, 70);    // 退出状态

    // 设置表头可排序
    m_model->setSortRole(Qt::UserRole);
}



void ThreadMonWindow::onStartClicked()
{
    DWORD bytesReturned = 0;
    COMMUNICATE_THREAD_MON input = {};
    input.OperateType = (OPERATE_TYPE)START_THREAD_MON;

    BOOL result = CommunicateDevice(&input, sizeof(input), nullptr, 0, &bytesReturned);

    if (result) {
        m_updateTimer->start();
        m_statusTimer->start();
        updateUIState(true);
        //QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("线程监控已启动"));

        // 记录开始时间
        m_startTime = QDateTime::currentDateTime();
    }
    else {
        DWORD error = GetLastError();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("启动线程监控失败 (错误代码: %1)").arg(error));
    }
}

void ThreadMonWindow::onStopClicked()
{
    DWORD bytesReturned = 0;
    COMMUNICATE_THREAD_MON input = {};
    input.OperateType = (OPERATE_TYPE)STOP_THREAD_MON;

    BOOL result = CommunicateDevice(&input, sizeof(input), nullptr, 0, &bytesReturned);

    if (result) {
        m_updateTimer->stop();
        m_statusTimer->stop();
        updateUIState(false);
        //QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("线程监控已停止"));
    }
    else {
        DWORD error = GetLastError();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("停止线程监控失败 (错误代码: %1)").arg(error));
    }
}

void ThreadMonWindow::onRefreshClicked()
{
    updateEvents();
}

void ThreadMonWindow::onClearClicked()
{
    if (m_model->rowCount() > 0) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, QStringLiteral("确认"),
            QStringLiteral("确定要清除所有%1条记录吗？").arg(m_model->rowCount()),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            m_model->removeRows(0, m_model->rowCount());
            m_currentEvents = 0;
        }
    }
}


void ThreadMonWindow::updateEvents()
{
    // 构造获取事件的请求
    DWORD bytesReturned = 0;
    COMMUNICATE_THREAD_MON input = {};
    input.OperateType = (OPERATE_TYPE)GET_EVENTS_THREAD_MON;

    // 分配缓冲区
    const ULONG maxEvents = 100;
    DWORD bufferSize = sizeof(THREAD_EVENT_PACKET) + (maxEvents - 1) * sizeof(THREAD_EVENT);
    PTHREAD_EVENT_PACKET packet = (PTHREAD_EVENT_PACKET)malloc(bufferSize);
    if (!packet) {
        return;
    }

    RtlZeroMemory(packet, bufferSize);
    packet->EventCount = maxEvents;
    packet->BufferSize = bufferSize;

    // 调用通信函数获取事件
    BOOL result = CommunicateDevice(&input, sizeof(input), packet, bufferSize, &bytesReturned);

    if (result && packet->EventCount > 0) {
        // 处理事件
        for (ULONG i = 0; i < packet->EventCount; i++) {
            addEventToTable(packet->Events[i]);
        }


        // 更新统计
        m_totalEvents += packet->EventCount;
    }

    free(packet);

    // 控制表格大小，避免内存占用过多
    if (m_model->rowCount() > 10000) {
        int removeCount = m_model->rowCount() - 8000;
        m_model->removeRows(0, removeCount);


    }
}

void ThreadMonWindow::updateStatus()
{
    if (m_updateTimer->isActive()) {
        qint64 uptime = m_startTime.secsTo(QDateTime::currentDateTime());
        QString uptimeStr = QString("%1时%2分%3秒")
            .arg(uptime / 3600, 2, 10, QChar('0'))
            .arg((uptime % 3600) / 60, 2, 10, QChar('0'))
            .arg(uptime % 60, 2, 10, QChar('0'));

    }
}

void ThreadMonWindow::addEventToTable(const THREAD_EVENT& event)
{
    QList<QStandardItem*> rowItems;

    // 事件类型
    QString typeStr = (event.Type == ThreadCreate) ? QStringLiteral("创建") : QStringLiteral("退出");
    QStandardItem* typeItem = new QStandardItem(typeStr);
    typeItem->setData(event.Type, Qt::UserRole);
    rowItems << typeItem;

    // 时间
    ULONG64 fileTime = (event.Type == ThreadCreate) ? event.CreateTime : event.ExitTime;
    QString timeStr = fileTimeToString(fileTime);
    QStandardItem* timeItem = new QStandardItem(timeStr);
    timeItem->setData(QVariant::fromValue(fileTime), Qt::UserRole);
    rowItems << timeItem;

    // 线程ID
    QStandardItem* tidItem = new QStandardItem(QString::number(event.ThreadId));
    tidItem->setData(QVariant::fromValue<unsigned int>(event.ThreadId), Qt::UserRole);
    rowItems << tidItem;

    // 进程ID
    QStandardItem* pidItem = new QStandardItem(QString::number(event.ProcessId));
    pidItem->setData(QVariant::fromValue<unsigned int>(event.ProcessId), Qt::UserRole);
    rowItems << pidItem;

    // 进程名（通过进程ID在应用层获取）
    QString processName = getProcessNameFromPid(event.ProcessId);
    rowItems << new QStandardItem(processName);

    // 映像路径
    QString imagePath = QString::fromWCharArray(event.ImagePath);
    rowItems << new QStandardItem(imagePath);

    // 模块名
    QString moduleName = QString::fromWCharArray(event.ModuleName);
    rowItems << new QStandardItem(moduleName);

    // 优先级
    QStandardItem* priorityItem = new QStandardItem(QString::number(event.Priority));
    priorityItem->setData(QVariant::fromValue<unsigned int>(event.Priority), Qt::UserRole);
    rowItems << priorityItem;

    // 基本优先级
    QStandardItem* basePriorityItem = new QStandardItem(QString::number(event.BasePriority));
    basePriorityItem->setData(QVariant::fromValue<unsigned int>(event.BasePriority), Qt::UserRole);
    rowItems << basePriorityItem;

    // 退出状态
    QString exitStatus;
    if (event.Type == ThreadExit) {
        exitStatus = QString("0x%1").arg(event.ExitStatus, 8, 16, QChar('0')).toUpper();
    }
    else {
        exitStatus = QStringLiteral("N/A");
    }
    rowItems << new QStandardItem(exitStatus);

    // 设置行颜色（统一配色方案：创建=浅绿，退出=浅红）
    QColor bgColor = (event.Type == ThreadCreate) ?
        QColor(204, 239, 206) :  // 创建 - 浅绿
        QColor(255, 199, 206);   // 退出 - 浅红

    for (QStandardItem* item : rowItems) {
        item->setBackground(QBrush(bgColor));
        item->setTextAlignment(Qt::AlignCenter);

        // 为可搜索列设置工具提示
        int col = rowItems.indexOf(item);
        if (col == 0 || col == 2 || col == 3 || col == 4) {
            item->setToolTip(item->text());
        }
    }

    m_model->appendRow(rowItems);
    m_currentEvents++;
}

QString ThreadMonWindow::fileTimeToString(ULONG64 fileTime)
{
    if (fileTime == 0) {
        return QString();
    }

    // FILETIME转换为QDateTime
    // FILETIME是从1601-01-01 00:00:00开始的100纳秒间隔
    ULONGLONG unixTime = (fileTime - 116444736000000000ULL) / 10000ULL; // 转换为毫秒
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(qint64(unixTime));

    return dateTime.toString("yyyy-MM-dd HH:mm:ss.zzz");
}

QString ThreadMonWindow::getProcessNameFromPid(ULONG pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) {
        return QString("N/A");
    }

    WCHAR buffer[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    QString name;

    if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
        QString fullPath = QString::fromWCharArray(buffer);
        name = fullPath.mid(fullPath.lastIndexOf('\\') + 1);
    }

    CloseHandle(hProcess);
    return name.isEmpty() ? QString("N/A") : name;
}

void ThreadMonWindow::updateUIState(bool isMonitoring)
{
    ui->ThreadMonStartBtn->setEnabled(!isMonitoring);
    ui->ThreadMonStopBtn->setEnabled(isMonitoring);
    ui->ThreadMonRefreshBtn->setEnabled(true);
    ui->ThreadMonClearBtn->setEnabled(true);

}

void ThreadMonWindow::onExportClicked()
{
    QModelIndexList selectedRows = ui->ThreadMon_TableView->selectionModel()->selectedRows();
    QList<int> rowsToExport;

    if (!selectedRows.isEmpty()) {
        for (const auto& index : selectedRows) {
            if (!rowsToExport.contains(index.row())) {
                rowsToExport.append(index.row());
            }
        }
    } else {
        for (int i = 0; i < m_model->rowCount(); i++) {
            rowsToExport.append(i);
        }
    }

    if (rowsToExport.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("没有可导出的数据！"));
        return;
    }

    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString filePath = desktopPath + "/ThreadMon.txt";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("无法创建文件: %1").arg(filePath));
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n\n";

    for (int col = 0; col < m_model->columnCount(); col++) {
        if (col > 0) out << "\t";
        out << m_model->horizontalHeaderItem(col)->text();
    }
    out << "\n";

    for (int row : rowsToExport) {
        for (int col = 0; col < m_model->columnCount(); col++) {
            if (col > 0) out << "\t";
            out << m_model->item(row, col)->text();
        }
        out << "\n";
    }

    file.close();
    QMessageBox::information(this, QStringLiteral("成功"),
        QStringLiteral("已导出 %1 条记录到 %2").arg(rowsToExport.size()).arg(filePath));
}