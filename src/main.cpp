#include "mainwindow.h"
#include <QApplication>
#include "config.h"

int main(int argc, char *argv[])
{
    ConfigReader::Instance("config.ini");
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();

} 
