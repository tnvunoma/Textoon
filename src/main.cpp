#include <QApplication>
#include <QDebug>
#include "mainwindow.h"
#include "textoon.h"
#include "lazybrush/lazybrush_components/window.h"


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QStringList args = QCoreApplication::arguments();

    if (args.size() < 2) {
        qDebug() << "Usage: program <input_folder>";
        return -1;
    }

    QString inputFolder = args[1];

    Textoon processor;
    processor.processFolder(inputFolder);

    MainWindow window;
    window.show();

    // lzwindow wnd;
    // wnd.show();

    return app.exec();
}
