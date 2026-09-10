#include "MainWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("SmartHomeClient"));
    application.setApplicationDisplayName(QStringLiteral("智能家居监控系统客户端"));

    shmsqt::MainWindow window;
    window.show();
    return application.exec();
}
