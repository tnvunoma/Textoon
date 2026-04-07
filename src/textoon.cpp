#include "textoon.h"

#include <QDir>
#include <QFileInfoList>
#include <QImage>
#include <QDebug>
#include <QColor>

Textoon::Textoon()
{
}

void Textoon::processFolder(const QString &inputFolder)
{
    outputFolder = inputFolder + "/output";
    QDir().mkpath(outputFolder);

    processFrames(inputFolder);
}

void Textoon::processFrames(const QString &inputFolder)
{
    QDir dir(inputFolder);

    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.bmp";

    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const QFileInfo &fileInfo : std::as_const(fileList))
    {
        QString inputPath = fileInfo.absoluteFilePath();

        QImage img(inputPath);

        if (img.isNull()) {
            qDebug() << "Failed to load:" << inputPath;
            continue;
        }

        QImage processed = processImage(img);

        QString baseName = fileInfo.completeBaseName(); // name_0000
        QString extension = fileInfo.suffix();

        QString outputPath = outputFolder + "/" + baseName + "." + extension;

        processed.save(outputPath);
    }

    qDebug() << "Done.";
}

QImage Textoon::processImage(const QImage &img)
{
    QImage result = img;

    // grayscale for now
    // TO DO: process frames for textoon
    for (int y = 0; y < result.height(); ++y)
    {
        for (int x = 0; x < result.width(); ++x)
        {
            QColor c = result.pixelColor(x, y);
            int gray = qGray(c.rgb());
            result.setPixelColor(x, y, QColor(gray, gray, gray));
        }
    }

    return result;
}
