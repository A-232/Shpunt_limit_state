#include <QApplication>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Расчёт шпунта методом предельных состояний");
    app.setOrganizationName("ShpuntCalc");

    MainWindow w;
    w.show();
    return app.exec();
}