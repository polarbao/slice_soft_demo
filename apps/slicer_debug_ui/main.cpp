#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow window(QDir::currentPath());
    if (QCoreApplication::arguments().contains("--self-test")) {
        return 0;
    }
    window.show();
    return app.exec();
}
