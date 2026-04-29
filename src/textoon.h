#ifndef TEXTOON_H
#define TEXTOON_H

#include <QString>
#include <QImage>
#include <Eigen/Dense>
#include "scribblecontext.h"


using namespace Eigen;

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
    ScribbleContext scribbleContext;

    QString outputFolder;

    void processFrames(const QString &inputFolder);

    QImage loadInitialScribbles(const QString& inputFolder);

    std::vector<std::vector<Textoon::UV>> initUV(int w, int h);

    std::vector<std::vector<QPointF>> buildMap(
        const std::vector<std::vector<int>>& map_x,
        const std::vector<std::vector<int>>& map_y);

    std::map<QRgb, std::vector<QPoint>> extractRegions(const QImage& segmentation);

    QImage warpImage(
        const QImage& src,
        const std::vector<std::vector<QPointF>>& W12);

    std::vector<std::vector<Textoon::UV>> transferUV(
        const std::vector<std::vector<UV>>& prevUV,
        const std::vector<std::vector<QPointF>>& W12,
        const std::map<QRgb, std::vector<QPoint>>& regions);

    QImage render(
        const QImage& sourceTexture,
        const std::vector<std::vector<UV>>& T);
};



#endif // TEXTOON_H
