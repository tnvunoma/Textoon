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
    // test UV transfer on dummy data
    return transferUV(img);
}

QImage Textoon::transferUV(const QImage &F1)
{
    int w = F1.width();
    int h = F1.height();

    QImage F2(w, h, QImage::Format_RGB32);
    F2.fill(Qt::black);

    std::vector<std::vector<Textoon::UV>> T1(h, std::vector<Textoon::UV>(w));
    std::vector<std::vector<Textoon::UV>> T2(h, std::vector<Textoon::UV>(w));
    std::vector<std::vector<bool>> assigned(h, std::vector<bool>(w, false));

    // Identity UVs
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            T1[y][x] = { (float)x / w, (float)y / h };
        }
    }

    // Define rotation (dummy W12)
    float angle = 20.0f * M_PI / 180.0f;
    float cx = w / 2.0f;
    float cy = h / 2.0f;

    auto rotate = [&](float x, float y) {
        float dx = x - cx;
        float dy = y - cy;

        float rx = cos(angle) * dx - sin(angle) * dy + cx;
        float ry = sin(angle) * dx + cos(angle) * dy + cy;

        return QPointF(rx, ry);
    };

    // Forward warp UVs (with intentional holes)
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            // Create artificial missing correspondences by skipping pixels
            if ((x + y) % 5 == 0) continue;

            QPointF qf = rotate(x, y);

            int qx = std::round(qf.x());
            int qy = std::round(qf.y());

            if (qx >= 0 && qx < w && qy >= 0 && qy < h)
            {
                if (!assigned[qy][qx])
                {
                    T2[qy][qx] = T1[y][x];
                    assigned[qy][qx] = true;
                }
                else
                {
                    T2[qy][qx].u = 0.5f * (T2[qy][qx].u + T1[y][x].u);
                    T2[qy][qx].v = 0.5f * (T2[qy][qx].v + T1[y][x].v);
                }
            }
        }
    }

    // Visualize UVs as color
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (assigned[y][x])
            {
                int r = T2[y][x].u * 255;
                int g = T2[y][x].v * 255;
                int b = 0;

                F2.setPixelColor(x, y, QColor(r, g, b));
            }
        }
    }

    // Visualize assigned vs missing
    // for (int y = 0; y < h; ++y)
    // {
    //     for (int x = 0; x < w; ++x)
    //     {
    //         if (assigned[y][x])
    //         {
    //             float u = T2[y][x].u;
    //             float v = T2[y][x].v;

    //             int sx = std::clamp(int(u * w), 0, w - 1);
    //             int sy = std::clamp(int(v * h), 0, h - 1);

    //             QColor color = F1.pixelColor(sx, sy);
    //             F2.setPixelColor(x, y, color);
    //         }
    //         else
    //         {
    //             // mark missing UVs clearly
    //             F2.setPixelColor(x, y, QColor(255, 0, 0));
    //         }
    //     }
    // }

    return F2;
}
