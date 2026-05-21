#include "FileMonWindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include"FileMonitor.h"
#include"PathConverter.h"
#include <QFileDialog>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QGroupBox>
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

    // 保护按钮
    btnProtect = new QPushButton(QStringLiteral("保护"));
    ui->horizontalLayout_2->insertWidget(5, btnProtect);
    connect(btnProtect, &QPushButton::clicked, this, &FileMonWindow::onProtectClicked);

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

// 发送文件保护请求到驱动
bool FileMonWindow::sendFileProtection(OPERATE_TYPE opType, const QString& ntPath)
{
    COMMUNICATE_FILE_PROTECT input = {};
    input.OperateType = opType;

    // 规范化路径：将正斜杠替换为反斜杠（Qt文件对话框可能返回正斜杠，
    // 导致与MiniFilter返回的NT路径格式不匹配）
    QString normalizedPath = ntPath;
    normalizedPath.replace(QChar('/'), QChar('\\'));

    // 将QString转换为WCHAR数组
    WCHAR filePath[520] = { 0 };
    int copyLen = normalizedPath.toWCharArray(filePath);
    if (copyLen >= 520) copyLen = 519;
    filePath[copyLen] = L'\0';
    RtlCopyMemory(input.FilePath, filePath, sizeof(filePath));

    DWORD bytesReturned = 0;
    return CommunicateDevice(&input, sizeof(input), nullptr, 0, &bytesReturned);
}

// 更新本地保护文件列表
void FileMonWindow::updateProtectedFileList(const QString& dosPath, const QString& ntPath,
    bool addDelete, bool addModify, bool addCopy,
    bool removeDelete, bool removeModify, bool removeCopy)
{
    // 查找或创建条目
    int idx = -1;
    for (int i = 0; i < m_protectedFiles.size(); i++) {
        if (m_protectedFiles[i].ntPath.compare(ntPath, Qt::CaseInsensitive) == 0) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        // 新条目
        ProtectedFileInfo info;
        info.dosPath = dosPath;
        info.ntPath = ntPath;
        info.deleteProtected = addDelete;
        info.modifyProtected = addModify;
        info.copyProtected = addCopy;
        if (addDelete || addModify || addCopy) {
            m_protectedFiles.append(info);
        }
    }
    else {
        // 更新现有条目
        if (addDelete) m_protectedFiles[idx].deleteProtected = true;
        if (addModify) m_protectedFiles[idx].modifyProtected = true;
        if (addCopy) m_protectedFiles[idx].copyProtected = true;
        if (removeDelete) m_protectedFiles[idx].deleteProtected = false;
        if (removeModify) m_protectedFiles[idx].modifyProtected = false;
        if (removeCopy) m_protectedFiles[idx].copyProtected = false;

        // 如果所有保护都已移除，删除该条目
        if (!m_protectedFiles[idx].deleteProtected &&
            !m_protectedFiles[idx].modifyProtected &&
            !m_protectedFiles[idx].copyProtected) {
            m_protectedFiles.removeAt(idx);
        }
    }
}

// 文件保护按钮点击
void FileMonWindow::onProtectClicked()
{
    // 创建保护对话框
    QDialog protectDialog(this);
    protectDialog.setWindowTitle(QStringLiteral("文件保护"));
    protectDialog.setMinimumWidth(550);
    protectDialog.setMinimumHeight(450);

    QVBoxLayout* mainLayout = new QVBoxLayout(&protectDialog);

    // 文件路径选择区域
    QGroupBox* fileGroup = new QGroupBox(QStringLiteral("选择文件"), &protectDialog);
    QHBoxLayout* fileLayout = new QHBoxLayout(fileGroup);
    QLineEdit* filePathEdit = new QLineEdit(&protectDialog);
    filePathEdit->setPlaceholderText(QStringLiteral("请选择要保护的文件..."));
    QPushButton* browseBtn = new QPushButton(QStringLiteral("浏览..."), &protectDialog);
    fileLayout->addWidget(filePathEdit);
    fileLayout->addWidget(browseBtn);
    mainLayout->addWidget(fileGroup);

    // 保护选项区域
    QGroupBox* optionGroup = new QGroupBox(QStringLiteral("保护选项"), &protectDialog);
    QVBoxLayout* optionLayout = new QVBoxLayout(optionGroup);

    QCheckBox* chkDelete = new QCheckBox(QStringLiteral("防止文件删除 - 阻止删除该文件"), &protectDialog);
    QCheckBox* chkModify = new QCheckBox(QStringLiteral("防止文件修改 - 阻止写入和重命名该文件"), &protectDialog);
    QCheckBox* chkCopy = new QCheckBox(QStringLiteral("防止文件复制 - 阻止读取该文件（文件将无法被读取）"), &protectDialog);

    optionLayout->addWidget(chkDelete);
    optionLayout->addWidget(chkModify);
    optionLayout->addWidget(chkCopy);

    // 取消保护按钮行
    QHBoxLayout* unprotectLayout = new QHBoxLayout();
    QPushButton* btnUnprotectDelete = new QPushButton(QStringLiteral("取消防删除"), &protectDialog);
    QPushButton* btnUnprotectModify = new QPushButton(QStringLiteral("取消防修改"), &protectDialog);
    QPushButton* btnUnprotectCopy = new QPushButton(QStringLiteral("取消防复制"), &protectDialog);
    btnUnprotectDelete->setEnabled(false);
    btnUnprotectModify->setEnabled(false);
    btnUnprotectCopy->setEnabled(false);
    unprotectLayout->addWidget(btnUnprotectDelete);
    unprotectLayout->addWidget(btnUnprotectModify);
    unprotectLayout->addWidget(btnUnprotectCopy);
    optionLayout->addLayout(unprotectLayout);

    mainLayout->addWidget(optionGroup);

    // 已保护文件列表
    QGroupBox* listGroup = new QGroupBox(QStringLiteral("已保护的文件"), &protectDialog);
    QVBoxLayout* listLayout = new QVBoxLayout(listGroup);
    QListWidget* protectedList = new QListWidget(&protectDialog);
    listLayout->addWidget(protectedList);
    mainLayout->addWidget(listGroup);

    // 底部按钮
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    QPushButton* btnSetProtect = new QPushButton(QStringLiteral("设置保护"), &protectDialog);
    QPushButton* btnClose = new QPushButton(QStringLiteral("关闭"), &protectDialog);
    btnSetProtect->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 6px 20px; font-weight: bold; }");
    btnClose->setStyleSheet("QPushButton { padding: 6px 20px; }");
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnSetProtect);
    bottomLayout->addWidget(btnClose);
    mainLayout->addLayout(bottomLayout);

    // 刷新已保护文件列表
    auto refreshProtectedList = [&]() {
        protectedList->clear();
        for (const auto& info : m_protectedFiles) {
            QStringList flags;
            if (info.deleteProtected) flags << QStringLiteral("防删除");
            if (info.modifyProtected) flags << QStringLiteral("防修改");
            if (info.copyProtected) flags << QStringLiteral("防复制");
            QString itemText = info.dosPath + "  [" + flags.join(", ") + "]";
            protectedList->addItem(itemText);
        }
    };
    refreshProtectedList();

    // 当文件路径变化时，更新复选框和取消按钮状态
    auto updateCheckBoxState = [&]() {
        QString dosPath = filePathEdit->text().trimmed();
        if (dosPath.isEmpty()) {
            chkDelete->setChecked(false);
            chkModify->setChecked(false);
            chkCopy->setChecked(false);
            btnUnprotectDelete->setEnabled(false);
            btnUnprotectModify->setEnabled(false);
            btnUnprotectCopy->setEnabled(false);
            return;
        }

        QString ntPath = PathConverter::instance().dosPathToNtPath(dosPath);
        bool foundDel = false, foundMod = false, foundCp = false;
        for (const auto& info : m_protectedFiles) {
            if (info.ntPath.compare(ntPath, Qt::CaseInsensitive) == 0) {
                foundDel = info.deleteProtected;
                foundMod = info.modifyProtected;
                foundCp = info.copyProtected;
                break;
            }
        }

        chkDelete->setChecked(foundDel);
        chkModify->setChecked(foundMod);
        chkCopy->setChecked(foundCp);
        btnUnprotectDelete->setEnabled(foundDel);
        btnUnprotectModify->setEnabled(foundMod);
        btnUnprotectCopy->setEnabled(foundCp);
    };

    // 浏览按钮
    connect(browseBtn, &QPushButton::clicked, [&]() {
        QString fileName = QFileDialog::getOpenFileName(&protectDialog,
            QStringLiteral("选择要保护的文件"),
            QString(),
            QStringLiteral("所有文件 (*.*)"));
        if (!fileName.isEmpty()) {
            filePathEdit->setText(fileName);
            updateCheckBoxState();
        }
    });

    // 文件路径手动修改时更新状态
    connect(filePathEdit, &QLineEdit::textChanged, [&](const QString&) {
        updateCheckBoxState();
    });

    // 列表项点击时填充文件路径
    connect(protectedList, &QListWidget::itemClicked, [&](QListWidgetItem* item) {
        int row = protectedList->row(item);
        if (row >= 0 && row < m_protectedFiles.size()) {
            filePathEdit->setText(m_protectedFiles[row].dosPath);
            updateCheckBoxState();
        }
    });

    // 设置保护按钮
    connect(btnSetProtect, &QPushButton::clicked, [&]() {
        QString dosPath = filePathEdit->text().trimmed();
        if (dosPath.isEmpty()) {
            QMessageBox::warning(&protectDialog, QStringLiteral("提示"), QStringLiteral("请先选择文件！"));
            return;
        }

        QString ntPath = PathConverter::instance().dosPathToNtPath(dosPath);
        if (ntPath == dosPath && !dosPath.startsWith("\\")) {
            QMessageBox::warning(&protectDialog, QStringLiteral("提示"),
                QStringLiteral("路径转换失败，无法识别的文件路径！"));
            return;
        }

        bool addDelete = chkDelete->isChecked();
        bool addModify = chkModify->isChecked();
        bool addCopy = chkCopy->isChecked();

        if (!addDelete && !addModify && !addCopy) {
            QMessageBox::warning(&protectDialog, QStringLiteral("提示"), QStringLiteral("请至少选择一种保护类型！"));
            return;
        }

        bool success = true;
        if (addDelete) {
            if (!sendFileProtection(PROTECT_FILE_DELETE, ntPath)) {
                success = false;
                QMessageBox::warning(&protectDialog, QStringLiteral("错误"),
                    QStringLiteral("设置防删除保护失败，请确认驱动已加载！"));
            }
        }
        if (addModify) {
            if (!sendFileProtection(PROTECT_FILE_MODIFY, ntPath)) {
                success = false;
                QMessageBox::warning(&protectDialog, QStringLiteral("错误"),
                    QStringLiteral("设置防修改保护失败，请确认驱动已加载！"));
            }
        }
        if (addCopy) {
            if (!sendFileProtection(PROTECT_FILE_COPY, ntPath)) {
                success = false;
                QMessageBox::warning(&protectDialog, QStringLiteral("错误"),
                    QStringLiteral("设置防复制保护失败，请确认驱动已加载！"));
            }
        }

        if (success) {
            // 查找当前保护状态
            bool curDel = false, curMod = false, curCp = false;
            for (const auto& info : m_protectedFiles) {
                if (info.ntPath.compare(ntPath, Qt::CaseInsensitive) == 0) {
                    curDel = info.deleteProtected;
                    curMod = info.modifyProtected;
                    curCp = info.copyProtected;
                    break;
                }
            }

            updateProtectedFileList(dosPath, ntPath, addDelete, addModify, addCopy, false, false, false);
            refreshProtectedList();
            updateCheckBoxState();

            QStringList protList;
            if (addDelete) protList << QStringLiteral("防删除");
            if (addModify) protList << QStringLiteral("防修改");
            if (addCopy) protList << QStringLiteral("防复制");
            QMessageBox::information(&protectDialog, QStringLiteral("成功"),
                QStringLiteral("已为文件 %1 设置保护：\n%2").arg(dosPath).arg(protList.join("、")));
        }
    });

    // 取消防删除
    connect(btnUnprotectDelete, &QPushButton::clicked, [&]() {
        QString dosPath = filePathEdit->text().trimmed();
        QString ntPath = PathConverter::instance().dosPathToNtPath(dosPath);
        if (sendFileProtection(UNPROTECT_FILE_DELETE, ntPath)) {
            updateProtectedFileList(dosPath, ntPath, false, false, false, true, false, false);
            refreshProtectedList();
            updateCheckBoxState();
            QMessageBox::information(&protectDialog, QStringLiteral("成功"),
                QStringLiteral("已取消文件 %1 的防删除保护").arg(dosPath));
        }
        else {
            QMessageBox::warning(&protectDialog, QStringLiteral("错误"),
                QStringLiteral("取消防删除保护失败，请确认驱动已加载！"));
        }
    });

    // 取消防修改
    connect(btnUnprotectModify, &QPushButton::clicked, [&]() {
        QString dosPath = filePathEdit->text().trimmed();
        QString ntPath = PathConverter::instance().dosPathToNtPath(dosPath);
        if (sendFileProtection(UNPROTECT_FILE_MODIFY, ntPath)) {
            updateProtectedFileList(dosPath, ntPath, false, false, false, false, true, false);
            refreshProtectedList();
            updateCheckBoxState();
            QMessageBox::information(&protectDialog, QStringLiteral("成功"),
                QStringLiteral("已取消文件 %1 的防修改保护").arg(dosPath));
        }
        else {
            QMessageBox::warning(&protectDialog, QStringLiteral("错误"),
                QStringLiteral("取消防修改保护失败，请确认驱动已加载！"));
        }
    });

    // 取消防复制
    connect(btnUnprotectCopy, &QPushButton::clicked, [&]() {
        QString dosPath = filePathEdit->text().trimmed();
        QString ntPath = PathConverter::instance().dosPathToNtPath(dosPath);
        if (sendFileProtection(UNPROTECT_FILE_COPY, ntPath)) {
            updateProtectedFileList(dosPath, ntPath, false, false, false, false, false, true);
            refreshProtectedList();
            updateCheckBoxState();
            QMessageBox::information(&protectDialog, QStringLiteral("成功"),
                QStringLiteral("已取消文件 %1 的防复制保护").arg(dosPath));
        }
        else {
            QMessageBox::warning(&protectDialog, QStringLiteral("错误"),
                QStringLiteral("取消防复制保护失败，请确认驱动已加载！"));
        }
    });

    // 关闭按钮
    connect(btnClose, &QPushButton::clicked, &protectDialog, &QDialog::accept);

    protectDialog.exec();
}