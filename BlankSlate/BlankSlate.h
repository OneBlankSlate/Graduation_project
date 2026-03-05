#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_BlankSlate.h"
#include<qstackedwidget.h>
class ProcessWindow;
class DriverModuleWindow;
class BlankSlate : public QMainWindow
{
    Q_OBJECT

public:
    BlankSlate(QWidget *parent = nullptr);
    ~BlankSlate();
private:
    Ui::BlankSlateClass ui;
private slots:

};
