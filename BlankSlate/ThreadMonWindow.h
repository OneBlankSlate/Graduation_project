// ThreadMonWindow.h
#pragma once

#include <QWidget>
#include "ui_ThreadMonWindow.h"
#include <QStandardItemModel>
#include <QTimer>
#include <QDateTime>
#include"ThreadMonCommon.h"
namespace Ui {
    class ThreadMonWindowClass;
}

class ThreadMonWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ThreadMonWindow(QWidget* parent = nullptr);
    ~ThreadMonWindow();

private:
    Ui::ThreadMonWindowClass* ui;
    QStandardItemModel* m_model;
    QTimer* m_updateTimer;
    QTimer* m_statusTimer;
    QPushButton* btnExport;

    qint64 m_totalEvents = 0;
    qint64 m_currentEvents = 0;
    QDateTime m_startTime;

    void setupTableView();
    void addEventToTable(const THREAD_EVENT& event);
    QString fileTimeToString(ULONG64 fileTime);
    void updateUIState(bool isMonitoring);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onRefreshClicked();
    void onClearClicked();
    void updateEvents();
    void updateStatus();
    void onExportClicked();  //导出选中内容
};