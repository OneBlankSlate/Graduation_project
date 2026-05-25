#pragma once

#include <QWidget>
#include "ui_ProcMonWindow.h"
#include<QStandardItemModel>
#include"ProcMonCommon.h"
class ProcMonWindow : public QWidget
{
	Q_OBJECT

public:
	ProcMonWindow(QWidget *parent = nullptr);
	~ProcMonWindow();
private:
    Ui::ProcMonWindowClass* ui;
    QStandardItemModel* m_model;
    QTimer* m_updateTimer;
    QPushButton* btnExport;

    void setupTableView();
    void addEventToTable(const PROCESS_EVENT& event);
    QString fileTimeToString(ULONG64 fileTime);
    void updateUIState(bool isMonitoring);
private slots:
    void onStartClicked();
    void onStopClicked();
    void onRefreshClicked();
    void onClearClicked();
    void updateEvents();
    void onExportClicked();  //导出选中内容
};

