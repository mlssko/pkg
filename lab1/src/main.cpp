#include <QApplication>

#include "view/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("ColorModelsLab");
    QApplication::setOrganizationName("Lab1-ColorModels");

    MainWindow window;
    window.show();

    return QApplication::exec();
}
