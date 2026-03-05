#include "BlankSlate.h"
#include"ProcessWindow.h"
#include"DriverModuleWindow.h"
#include"ProcessHelper.h"

BlankSlate::BlankSlate(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    //m_processWindow = new ProcessWindow(this);
    //m_driverModuleWindow = new DriverModuleWindow(this);

    ////将两个widget添加至stackedWidget
    //ui.stackedWidget->addWidget(m_processWindow);
    //ui.stackedWidget->addWidget(m_driverModuleWindow);
    ////设置stackedWidget当前展示页面
    //ui.stackedWidget->setCurrentIndex(2);
    // 设置主界面图标
    QIcon icon(":/BlankSlate/resource/blue.png");
    this->setWindowIcon(icon);

    ui.tabWidget->clear();   //清空默认的tab标签
    ui.tabWidget->addTab(new ProcessWindow, QStringLiteral("进程"));
    ui.tabWidget->addTab(new DriverModuleWindow, QStringLiteral("驱动模块"));

}

BlankSlate::~BlankSlate()
{}
