#include "BlankSlate.h"
#include <QtWidgets/QApplication>
#include<QTextCodec>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    BlankSlate w;
    w.show();
    return a.exec();
}
