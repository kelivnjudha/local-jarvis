#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Local Jarvis");
    QApplication::setOrganizationName("Local Jarvis");

    MainWindow window;
    window.show();

    return QApplication::exec();
}
