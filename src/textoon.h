#ifndef TEXTOON_H
#define TEXTOON_H

#include <QString>
#include <QImage>

class Textoon
{
public:
    Textoon();

    void processFolder(const QString &inputFolder);

private:
    QString outputFolder;

    void processFrames(const QString &inputFolder);
    QImage processImage(const QImage &img);
};

#endif // TEXTOON_H
