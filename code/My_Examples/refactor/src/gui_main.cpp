#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    MainWindow window;
    window.setWindowTitle("Multi-Camera Stream Viewer");
    window.resize(800, 600);
    window.show();

    return app.exec();
}