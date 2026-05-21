// ModuleMonWindow.cpp
#include "ModuleMonWindow.h"
#include "ui_ModuleMonWindow.h"
#include <QDateTime>
#include <QMessageBox>
#include <QHeaderView>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include "ModuleMonCommon.h"
#include "ModuleMonitor.h"
#include "PathConverter.h"
#include <psapi.h>

ModuleMonWindow::ModuleMonWindow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ModuleMonWindowClass)
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
    connect(ui->ModuleMonStartBtn, &QPushButton::clicked, this, &ModuleMonWindow::onStartClicked);
    connect(ui->ModuleMonStopBtn, &QPushButton::clicked, this, &ModuleMonWindow::onStopClicked);
    connect(ui->ModuleMonRefreshBtn, &QPushButton::clicked, this, &ModuleMonWindow::onRefreshClicked);
    connect(ui->ModuleMonClearBtn, &QPushButton::clicked, this, &ModuleMonWindow::onClearClicked);

    // 导出按钮
    btnExport = new QPushButton(QStringLiteral("导出"));
    ui->horizontalLayout->insertWidget(4, btnExport);
    connect(btnExport, &QPushButton::clicked, this, &ModuleMonWindow::onExportClicked);

    // 设置定时器
    m_updateTimer->setInterval(1000); // 1秒更新一次事件
    connect(m_updateTimer, &QTimer::timeout, this, &ModuleMonWindow::updateEvents);

    // 状态更新定时器
    m_statusTimer->setInterval(5000); // 5秒更新一次状态
    connect(m_statusTimer, &QTimer::timeout, this, &ModuleMonWindow::updateStatus);

    // 初始状态
    updateUIState(false);
}

ModuleMonWindow::~ModuleMonWindow()
{
    // 停止监控
    if (ui->ModuleMonStopBtn->isEnabled()) {
        onStopClicked();
    }

    if (m_model) {
        m_model->deleteLater();
    }

    delete ui;
}

void ModuleMonWindow::setupTableView()
{
    // 设置表头
    QStringList headers = {
        QStringLiteral("事件类型"),
        QStringLiteral("时间"),
        QStringLiteral("进程ID"),
        QStringLiteral("进程名"),
        QStringLiteral("映像路径"),
        QStringLiteral("映像名"),
        QStringLiteral("加载基地址"),
        QStringLiteral("映像大小")
    };

    m_model->setHorizontalHeaderLabels(headers);

    // 设置表格属性
    ui->ModuleMon_TableView->setModel(m_model);
    ui->ModuleMon_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->ModuleMon_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->ModuleMon_TableView->horizontalHeader()->setStretchLastSection(true);
    ui->ModuleMon_TableView->setSortingEnabled(true);

    // 设置列宽
    ui->ModuleMon_TableView->setColumnWidth(0, 60);    // 事件类型
    ui->ModuleMon_TableView->setColumnWidth(1, 200);   // 时间
    ui->ModuleMon_TableView->setColumnWidth(2, 60);    // 进程ID
    ui->ModuleMon_TableView->setColumnWidth(3, 200);   // 进程名
    ui->ModuleMon_TableView->setColumnWidth(4, 600);   // 映像路径
    ui->ModuleMon_TableView->setColumnWidth(5, 100);   // 映像名
    ui->ModuleMon_TableView->setColumnWidth(6, 120);   // 加载基地址
    ui->ModuleMon_TableView->setColumnWidth(7, 80);    // 映像大小

    // 设置表头可排序
    m_model->setSortRole(Qt::UserRole);
}

void ModuleMonWindow::onStartClicked()
{
    DWORD bytesReturned = 0;
    COMMUNICATE_MODULE_MON input = {};
    input.OperateType = (OPERATE_TYPE)START_MODULE_MON;

    BOOL result = CommunicateDevice(&input, sizeof(input), nullptr, 0, &bytesReturned);

    if (result) {
        m_updateTimer->start();
        m_statusTimer->start();
        updateUIState(true);

        // 记录开始时间
        m_startTime = QDateTime::currentDateTime();
    }
    else {
        DWORD error = GetLastError();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("启动映像加载监控失败 (错误代码: %1)").arg(error));
    }
}

void ModuleMonWindow::onStopClicked()
{
    DWORD bytesReturned = 0;
    COMMUNICATE_MODULE_MON input = {};
    input.OperateType = (OPERATE_TYPE)STOP_MODULE_MON;

    BOOL result = CommunicateDevice(&input, sizeof(input), nullptr, 0, &bytesReturned);

    if (result) {
        m_updateTimer->stop();
        m_statusTimer->stop();
        updateUIState(false);
    }
    else {
        DWORD error = GetLastError();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("停止映像加载监控失败 (错误代码: %1)").arg(error));
    }
}

void ModuleMonWindow::onRefreshClicked()
{
    updateEvents();
}

void ModuleMonWindow::onClearClicked()
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

void ModuleMonWindow::updateEvents()
{
    // 构造获取事件的请求
    DWORD bytesReturned = 0;
    COMMUNICATE_MODULE_MON input = {};
    input.OperateType = (OPERATE_TYPE)GET_EVENTS_MODULE_MON;

    // 分配缓冲区
    const ULONG maxEvents = 100;
    DWORD bufferSize = sizeof(MODULE_EVENT_PACKET) + (maxEvents - 1) * sizeof(MODULE_EVENT);
    PMODULE_EVENT_PACKET packet = (PMODULE_EVENT_PACKET)malloc(bufferSize);
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

void ModuleMonWindow::updateStatus()
{
    if (m_updateTimer->isActive()) {
        qint64 uptime = m_startTime.secsTo(QDateTime::currentDateTime());
        QString uptimeStr = QString("%1时%2分%3秒")
            .arg(uptime / 3600, 2, 10, QChar('0'))
            .arg((uptime % 3600) / 60, 2, 10, QChar('0'))
            .arg(uptime % 60, 2, 10, QChar('0'));
    }
}

void ModuleMonWindow::addEventToTable(const MODULE_EVENT& event)
{
    QList<QStandardItem*> rowItems;

    // 事件类型
    QString typeStr = (event.Type == ImageLoad) ? QStringLiteral("加载") : QStringLiteral("卸载");
    QStandardItem* typeItem = new QStandardItem(typeStr);
    typeItem->setData(event.Type, Qt::UserRole);
    rowItems << typeItem;

    // 时间
    QString timeStr = fileTimeToString(event.LoadTime);
    QStandardItem* timeItem = new QStandardItem(timeStr);
    timeItem->setData(QVariant::fromValue(event.LoadTime), Qt::UserRole);
    rowItems << timeItem;

    // 进程ID
    QStandardItem* pidItem = new QStandardItem(QString::number(event.ProcessId));
    pidItem->setData(QVariant::fromValue<unsigned int>(event.ProcessId), Qt::UserRole);
    rowItems << pidItem;

    // 进程名（通过进程ID在应用层获取）
    QString processName = getProcessNameFromPid(event.ProcessId);
    rowItems << new QStandardItem(processName);

    // 映像路径（NT路径转DOS路径）
    QString imagePath = PathConverter::instance().ntPathToDosPath(
        QString::fromWCharArray(event.ImagePath));
    rowItems << new QStandardItem(imagePath);

    // 映像名
    QString imageName = QString::fromWCharArray(event.ImageName);
    rowItems << new QStandardItem(imageName);

    // 加载基地址
    QString baseAddr = QString("0x%1").arg(event.ImageBase, sizeof(ULONG64) * 2, 16, QChar('0')).toUpper();
    rowItems << new QStandardItem(baseAddr);

    // 映像大小
    QString imageSize = QString("0x%1").arg(event.ImageSize, 8, 16, QChar('0')).toUpper();
    rowItems << new QStandardItem(imageSize);

    // 设置行颜色（统一配色方案：加载=浅绿，卸载=浅红）
    QColor bgColor = (event.Type == ImageLoad) ?
        QColor(204, 239, 206) :  // 加载 - 浅绿
        QColor(255, 199, 206);   // 卸载 - 浅红

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

QString ModuleMonWindow::fileTimeToString(ULONG64 fileTime)
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

QString ModuleMonWindow::getProcessNameFromPid(ULONG pid)
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

void ModuleMonWindow::updateUIState(bool isMonitoring)
{
    ui->ModuleMonStartBtn->setEnabled(!isMonitoring);
    ui->ModuleMonStopBtn->setEnabled(isMonitoring);
    ui->ModuleMonRefreshBtn->setEnabled(true);
    ui->ModuleMonClearBtn->setEnabled(true);
}

void ModuleMonWindow::onExportClicked()
{
    QModelIndexList selectedRows = ui->ModuleMon_TableView->selectionModel()->selectedRows();
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
    QString filePath = desktopPath + "/ModuleMon.txt";

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
