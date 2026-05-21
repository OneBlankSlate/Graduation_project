#include "FileMonWindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include"FileMonitor.h"
#include"PathConverter.h"
#pragma comment(lib, "Psapi.lib")
FileMonWindow::FileMonWindow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::FileMonWindowClass)
    , m_updateTimer(new QTimer(this))
{
    ui->setupUi(this);

    // 初始化模型
    m_model = new QStandardItemModel(this);
    setupTableView();

    // 连接按钮信号
    connect(ui->StartMonBtn, &QPushButton::clicked, this, &FileMonWindow::onStartClicked);
    connect(ui->StopMonBtn, &QPushButton::clicked, this, &FileMonWindow::onStopClicked);
    connect(ui->RefreshLogBtn, &QPushButton::clicked, this, &FileMonWindow::onRefreshClicked);
    connect(ui->ClearLogBtn, &QPushButton::clicked, this, &FileMonWindow::onClearClicked);

    // 导出按钮
    btnExport = new QPushButton(QStringLiteral("导出"));
    ui->horizontalLayout_2->insertWidget(4, btnExport);
    connect(btnExport, &QPushButton::clicked, this, &FileMonWindow::onExportClicked);

    // 设置定时器
    m_updateTimer->setInterval(1000); // 1秒刷新一次
    connect(m_updateTimer, &QTimer::timeout, this, &FileMonWindow::updateEvents);

    // 初始状态
    updateUIState(false);
}

FileMonWindow::~FileMonWindow()
{
    if (m_isMonitoring) {
        onStopClicked(); // 确保停止监控
    }
    delete ui;
}

void FileMonWindow::setupTableView()
{
    // 设置表头
    QStringList headers = {
        QStringLiteral("时间"),
        QStringLiteral("操作"),
        QStringLiteral("PID"),
        QStringLiteral("进程名"),
        QStringLiteral("文件路径")
    };
    m_model->setHorizontalHeaderLabels(headers);

    // 设置表格属性
    ui->FileMon_TableView->setModel(m_model);
    ui->FileMon_TableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->FileMon_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->FileMon_TableView->horizontalHeader()->setStretchLastSection(true);
    ui->FileMon_TableView->setSortingEnabled(true);

    // 设置列宽
    ui->FileMon_TableView->setColumnWidth(0, 180);   // 时间
    ui->FileMon_TableView->setColumnWidth(1, 100);   // 操作
    ui->FileMon_TableView->setColumnWidth(2, 80);    // PID
    ui->FileMon_TableView->setColumnWidth(3, 150);   // 进程名
    ui->FileMon_TableView->setColumnWidth(4, 400);    // 文件路径
}

void FileMonWindow::updateUIState(bool isMonitoring)
{
    m_isMonitoring = isMonitoring;
    ui->StartMonBtn->setEnabled(!isMonitoring);
    ui->StopMonBtn->setEnabled(isMonitoring);
    ui->RefreshLogBtn->setEnabled(true);
    ui->ClearLogBtn->setEnabled(true);

    if (isMonitoring) {
        m_updateTimer->start();
    }
    else {
        m_updateTimer->stop();
    }
}

QString FileMonWindow::fileTimeToString(ULONGLONG fileTime)
{
    // 将 Windows FILETIME 转换为 QDateTime
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)(fileTime & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD)(fileTime >> 32);

    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);

    QDateTime datetime;
    datetime.setDate(QDate(st.wYear, st.wMonth, st.wDay));
    datetime.setTime(QTime(st.wHour, st.wMinute, st.wSecond, st.wMilliseconds));

    return datetime.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

QString FileMonWindow::fileEventTypeToString(FILE_EVENT_TYPE type)
{
    switch (type) {
    case FileCreateOrOpen:
        return QStringLiteral("创建/打开");
    case FileRead:
        return QStringLiteral("读取");
    case FileWrite:
        return QStringLiteral("写入");
    case FileDelete:
        return QStringLiteral("删除");
    case FileRename:
        return QStringLiteral("重命名");
    case FileSetInfo:
        return QStringLiteral("设置信息");
    default:
        return QStringLiteral("未知");
    }
}

void FileMonWindow::onStartClicked()
{
    DWORD bytesReturned = 0;
    COMMUNICATE_FILE_MON input = {};
    input.OperateType = START_FILE_MON;
    BOOL result = CommunicateDevice(&input, sizeof(input), nullptr, 0, &bytesReturned);

    if (result) {
        m_updateTimer->start();
        updateUIState(true);
    }
    else {
        QMessageBox::warning(this, "错误", "启动进程监控失败");
    }
}

void FileMonWindow::onStopClicked()
{
    DWORD bytesReturned = 0;
    COMMUNICATE_FILE_MON input = {};
    input.OperateType = STOP_FILE_MON;
    BOOL result = CommunicateDevice(&input, sizeof(input), nullptr, 0, &bytesReturned);

    if (result) {
        m_updateTimer->stop();
        updateUIState(false);
    }
    else {
        QMessageBox::warning(this, "错误", "停止进程监控失败");
    }
}

void FileMonWindow::onRefreshClicked()
{
    updateEvents();
}

void FileMonWindow::onClearClicked()
{
    m_model->removeRows(0, m_model->rowCount());
}

void FileMonWindow::updateEvents()
{
    if (!m_isMonitoring) {
        return;
    }

    //
    // 分配缓冲区
    //
    DWORD bufferSize =
        FILE_EVENT_PACKET_SIZE;

    BYTE* buffer =
        new BYTE[bufferSize];

    if (!buffer) {
        return;
    }

    //
    // 设置事件包
    //
    PFILE_EVENT_PACKET packet =
        reinterpret_cast<PFILE_EVENT_PACKET>(
            buffer);

    packet->EventCount =
        MAX_FILE_EVENTS;

    packet->BufferSize =
        bufferSize;

    DWORD bytesReturned = 0;

    COMMUNICATE_FILE_MON input = {};

    input.OperateType =
        GET_EVENTS_FILE_MON;

    //
    // 与驱动通信
    //
    if (CommunicateDevice(
        &input,
        sizeof(COMMUNICATE_FILE_MON),
        buffer,
        bufferSize,
        &bytesReturned))
    {
        if (bytesReturned > 0)
        {
            DWORD eventCount =
                bytesReturned /
                sizeof(FILE_EVENT);

            for (DWORD i = 0;
                i < eventCount &&
                i < packet->EventCount;
                i++)
            {
                const FILE_EVENT& event =
                    packet->Events[i];

                QList<QStandardItem*> rowItems;

                //
                // 时间
                //
                QString timeStr =
                    fileTimeToString(
                        event.TimeStamp);

                rowItems.append(
                    new QStandardItem(
                        timeStr));

                //
                // 操作类型
                //
                QString operationStr =
                    fileEventTypeToString(
                        event.Type);

                rowItems.append(
                    new QStandardItem(
                        operationStr));

                //
                // PID
                //
                rowItems.append(
                    new QStandardItem(
                        QString::number(
                            event.ProcessId)));

                //
                // 进程名
                //
                QString processName =
                    "Unknown";

                //
                // PID 4 特殊处理
                //
                if (event.ProcessId == 4)
                {
                    processName = "System";
                }
                else
                {
                    HANDLE hProcess =
                        OpenProcess(
                            PROCESS_QUERY_LIMITED_INFORMATION,
                            FALSE,
                            event.ProcessId);

                    if (hProcess)
                    {
                        WCHAR processPath[MAX_PATH] = { 0 };

                        DWORD pathSize =
                            MAX_PATH;

                        if (QueryFullProcessImageNameW(
                            hProcess,
                            0,
                            processPath,
                            &pathSize))
                        {
                            QFileInfo fileInfo(
                                QString::fromWCharArray(
                                    processPath));

                            processName =
                                fileInfo.fileName();
                        }

                        CloseHandle(hProcess);
                    }
                }

                rowItems.append(
                    new QStandardItem(
                        processName));

                //
                // 文件路径（NT路径转DOS路径）
                //
                QString filePath =
                    PathConverter::instance().ntPathToDosPath(
                        QString::fromWCharArray(
                            event.FilePath));

                rowItems.append(
                    new QStandardItem(
                        filePath));


                //
                // 添加到模型
                //

                // 设置行颜色（统一配色方案，按事件类型区分）
                QColor bgColor;
                switch (event.Type) {
                case FileCreateOrOpen:
                    bgColor = QColor(204, 239, 206);   // 创建/打开 - 浅绿
                    break;
                case FileRead:
                    bgColor = QColor(189, 215, 238);   // 读取 - 浅蓝
                    break;
                case FileWrite:
                    bgColor = QColor(252, 228, 181);   // 写入 - 浅橙
                    break;
                case FileDelete:
                    bgColor = QColor(255, 199, 206);   // 删除 - 浅红
                    break;
                case FileRename:
                    bgColor = QColor(217, 210, 233);   // 重命名 - 浅紫
                    break;
                case FileSetInfo:
                    bgColor = QColor(214, 220, 228);   // 设置信息 - 浅灰蓝
                    break;
                default:
                    bgColor = QColor(255, 255, 255);   // 未知 - 白色
                    break;
                }
                for (QStandardItem* item : rowItems) {
                    item->setBackground(QBrush(bgColor));
                }

                m_model->appendRow(
                    rowItems);
            }

            //
            // 自动滚动
            //
            if (eventCount > 0)
            {
                ui->FileMon_TableView
                    ->scrollToBottom();
            }
        }
    }

    delete[] buffer;
}

void FileMonWindow::onExportClicked()
{
    QModelIndexList selectedRows = ui->FileMon_TableView->selectionModel()->selectedRows();
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
    QString filePath = desktopPath + "/FileMon.txt";

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