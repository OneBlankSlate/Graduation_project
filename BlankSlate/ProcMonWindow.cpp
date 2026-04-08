#include "ProcMonWindow.h"
#include "ui_ProcMonWindow.h"
#include <QDateTime>
#include <QMessageBox>
#include <QHeaderView>
#include <QDebug>
#include<QTimer>
#include"IoControlHelper.h"
#include"ProcMonitor.h"
ProcMonWindow::ProcMonWindow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ProcMonWindowClass)
    , m_updateTimer(new QTimer(this))
    , m_statusTimer(new QTimer(this))
{
    ui->setupUi(this);

    // 设置表格模型
    m_model = new QStandardItemModel(this);
    setupTableView();

    // 连接按钮信号
    connect(ui->btnStart, &QPushButton::clicked, this, &ProcMonWindow::onStartClicked);
    connect(ui->btnStop, &QPushButton::clicked, this, &ProcMonWindow::onStopClicked);
    connect(ui->btnRefresh, &QPushButton::clicked, this, &ProcMonWindow::onRefreshClicked);
    connect(ui->btnClear, &QPushButton::clicked, this, &ProcMonWindow::onClearClicked);

    // 设置定时器
    m_updateTimer->setInterval(1000); // 1秒更新一次事件
    //m_statusTimer->setInterval(2000); // 2秒更新一次状态

    connect(m_updateTimer, &QTimer::timeout, this, &ProcMonWindow::updateEvents);
    //connect(m_statusTimer, &QTimer::timeout, this, &ProcMonWindow::updateStatus);

    // 初始状态
    updateUIState(false);
}

ProcMonWindow::~ProcMonWindow()
{
    delete ui;
}

void ProcMonWindow::setupTableView()
{
    // 设置表头
    QStringList headers = { QStringLiteral("事件ID"), QStringLiteral("类型"), QStringLiteral("时间"), QStringLiteral("进程ID"), QStringLiteral("父进程ID"),QStringLiteral("进程名称"),QStringLiteral("映像路径"),QStringLiteral("命令行")};
    m_model->setHorizontalHeaderLabels(headers);

    // 设置表格属性
    ui->ProcMon_TableView->setModel(m_model);
    ui->ProcMon_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->ProcMon_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->ProcMon_TableView->horizontalHeader()->setStretchLastSection(true);
    ui->ProcMon_TableView->setSortingEnabled(true);

    // 设置列宽
    ui->ProcMon_TableView->setColumnWidth(0, 80);   // 事件ID
    ui->ProcMon_TableView->setColumnWidth(1, 80);   // 类型
    ui->ProcMon_TableView->setColumnWidth(2, 180);  // 时间
    ui->ProcMon_TableView->setColumnWidth(3, 80);   // 进程ID
    ui->ProcMon_TableView->setColumnWidth(4, 80);   // 父进程ID
    ui->ProcMon_TableView->setColumnWidth(5, 200);  // 进程名称
    ui->ProcMon_TableView->setColumnWidth(6, 300);  // 映像路径
    ui->ProcMon_TableView->setColumnWidth(7, 300);  // 命令行
}

void ProcMonWindow::onStartClicked()
{
    __debugbreak();
    DWORD bytesReturned = 0;
    COMMUNICATE_PROCESS_MON input = {};
    input.OperateType = START_PROC_MON;
    BOOL result = CommunicateDevice(&input, sizeof(input), nullptr, 0, &bytesReturned);

    if (result) {
        m_updateTimer->start();
        m_statusTimer->start();
        updateUIState(true);
    }
    else {
        QMessageBox::warning(this, "错误", "启动进程监控失败");
    }
}

void ProcMonWindow::onStopClicked()
{
    DWORD bytesReturned = 0;
    COMMUNICATE_PROCESS_MON input = {};
    input.OperateType = STOP_PROC_MON;
    BOOL result = CommunicateDevice(&input, sizeof(input), nullptr, 0, &bytesReturned);

    if (result) {
        m_updateTimer->stop();
        m_statusTimer->stop();
        updateUIState(false);
    }
    else {
        QMessageBox::warning(this, "错误", "停止进程监控失败");
    }
}

void ProcMonWindow::onRefreshClicked()
{
    updateEvents();
    //updateStatus();
}

void ProcMonWindow::onClearClicked()
{
    m_model->removeRows(0, m_model->rowCount());
}

void ProcMonWindow::updateEvents()
{
    // 分配缓冲区获取事件
    const ULONG maxEvents = 100;
    DWORD bufferSize = sizeof(EVENT_PACKET) + (maxEvents - 1) * sizeof(PROCESS_EVENT);
    PEVENT_PACKET packet = (PEVENT_PACKET)malloc(bufferSize);
    packet->BufferSize = bufferSize;
    packet->EventCount = maxEvents;
    DWORD bytesReturned = 0;
    COMMUNICATE_PROCESS_MON input = {};
    input.OperateType = GET_EVENTS_PROC_MON;
    BOOL result = CommunicateDevice(&input, sizeof(input), packet, bufferSize, &bytesReturned);
    if (result && bytesReturned >= sizeof(ULONG)) {
        ULONG eventCount = packet->EventCount;

        for (ULONG i = 0; i < eventCount; i++) {
            addEventToTable(packet->Events[i]);
        }

        // 自动滚动到最新事件
        if (eventCount > 0) {
            ui->ProcMon_TableView->scrollToBottom();
        }
    }
}

void ProcMonWindow::addEventToTable(const PROCESS_EVENT& event)
{
    QList<QStandardItem*> rowItems;

    // 事件ID
    rowItems << new QStandardItem(QString::number(event.EventId));

    // 事件类型
    QString typeStr = (event.Type == ProcessCreate) ? QStringLiteral("创建") : QStringLiteral("退出");
    rowItems << new QStandardItem(typeStr);

    // 时间戳
    ULONG64 fileTime = (event.Type == ProcessCreate) ? event.CreateTime : event.ExitTime;
    QString timeStr = fileTimeToString(fileTime);
    rowItems << new QStandardItem(timeStr);

    // 进程ID
    rowItems << new QStandardItem(QString::number(event.ProcessId));

    // 父进程ID
    rowItems << new QStandardItem(QString::number(event.ParentProcessId));

    // 进程名称
    rowItems << new QStandardItem(QString::fromWCharArray(event.ImageName));

    // 映像路径
    rowItems << new QStandardItem(QString::fromWCharArray(event.ImagePath));

    // 命令行
    rowItems << new QStandardItem(QString::fromWCharArray(event.CommandLine));

    // 设置行背景色
    QColor bgColor = (event.Type == ProcessCreate) ?
        QColor(220, 255, 220) : QColor(255, 220, 220);

    for (QStandardItem* item : rowItems) {
        item->setBackground(QBrush(bgColor));
    }

    m_model->appendRow(rowItems);

    // 限制表格行数，避免内存占用过大
    if (m_model->rowCount() > 1000) {
        m_model->removeRow(0);
    }
}



QString ProcMonWindow::fileTimeToString(ULONG64 fileTime)
{
    if (fileTime == 0) {
        return QString();
    }

    // 将FILETIME转换为QDateTime
    // FILETIME是自1601-01-01 00:00:00以来的100纳秒间隔数
    // 转换为自1970-01-01 00:00:00以来的毫秒数
    ULONGLONG unixTime = (fileTime - 116444736000000000ULL) / 10000ULL;
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(unixTime);

    return dateTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

void ProcMonWindow::updateUIState(bool isMonitoring)
{
    ui->btnStart->setEnabled(!isMonitoring);
    ui->btnStop->setEnabled(isMonitoring);
    ui->btnRefresh->setEnabled(true);
    ui->btnClear->setEnabled(true);
}