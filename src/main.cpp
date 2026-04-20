#include <QApplication>
#include <QDebug>
#include "mainwindow.h"
#include "textoon.h"
#include "src/scribblecontext.h"


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QStringList args = QCoreApplication::arguments();

    if (args.size() < 2) {
        qDebug() << "Usage: program <input_folder>";
        return -1;
    }

    QString inputFolder = args[1];


    ScribbleContext* scribble_context = new ScribbleContext();

    Textoon processor;
    processor.processFolder(inputFolder);

    MainWindow window = MainWindow(nullptr, scribble_context);
    window.show();

    return app.exec();
}
