#include <QApplication>
#include <QDebug>
#include "mainwindow.h"
#include "textoon.h"
#include "src/scribblecontext.h"
#include <iostream>
#include <QDir>


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

    lzwindow* lazyb_window = new lzwindow(scribble_context);
    lazyb_window->show();





    /*

    QVector<QImage> segmentation_images;
    QVector<ScribbleInfo> svd{scribble_context->saved_scribbles};
    for (const ScribbleInfo& scrib : svd){
        segmentation_images.push_back(scribble_context->colorize(scrib));
        QDir().mkpath("saved_scribbles");

        QString filename = "saved_scribbles/segmentation_" + QString::number(scrib.label()) + ".png";

        if (!segmentation_images.back().save(filename)) {
            std::cerr << "Failed to save scribbles." << std::endl;
        }
    }


        QVector<ScribbleInfo> scribbles{
                                        scribble_context->extractScribblesFromQImage("saved_scribbles/combined_scribbles.png")};
        QVector<QImage> segmentation_images;

        for (const ScribbleInfo& scrib : scribbles){
            // QDir().mkpath("saved_scribbles");

            // QString filename = "saved_scribbles/scribble_" + QString::number(scrib.label()) + ".png";

            // if (!scrib.scrib_image.save(filename)) {
            //     std::cerr << "Failed to save scribble." << std::endl;
            // }
            segmentation_images.push_back(scribble_context->colorize(scrib));
            QDir().mkpath("saved_scribbles");

            QString filename = "saved_scribbles/segmentation_" + QString::number(scrib.label()) + ".png";

            if (!segmentation_images.back().save(filename)) {
                std::cerr << "Failed to save scribbles." << std::endl;
            }
        }
    */

    // MainWindow window = MainWindow(nullptr, scribble_context);
    // window.show();

    return app.exec();
}
