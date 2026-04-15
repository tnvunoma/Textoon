#ifndef TEXTOON_H
#define TEXTOON_H

#include <QString>
#include <QImage>

class Textoon
{
public:
    Textoon();

    void processFolder(const QString &inputFolder);

    struct UV {
        float u;
        float v;
    };

private:


    QString outputFolder;

    void processFrames(const QString &inputFolder);
    QImage processImage(const QImage &img);

    QImage transferUV(const QImage &F1);

};

#endif // TEXTOON_H
