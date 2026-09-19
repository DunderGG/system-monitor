#include <QApplication>
#include <QMainWindow>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    QMainWindow mainWindow;
    mainWindow.show();

    return application.exec();
}
