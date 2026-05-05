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
    Textoon(ScribbleContext* scribble_context) : scribbleContext(scribble_context) {};

    void processFolder(const QString &inputFolder, const QString &anim_name);

    struct UV {
        float u;
        float v;
    };

private:
    ScribbleContext* scribbleContext;

    QString outputFolder;
    QString anim_nm;

    void processFrames(const QString &inputFolder);

    QImage loadInitialScribbles(const QString& inputFolder);

    std::vector<std::vector<Textoon::UV>> initUV(int w, int h);

    QImage textureInitFrame(
        const QImage&                              lineArt,
        const QImage&                              coloredLineArt,
        const std::map<QRgb, std::vector<QPoint>>& regions,
        const QMap<QRgb, QImage>&                  textureMap);

    std::vector<std::vector<QPointF>> buildMap(
        const std::vector<std::vector<int>>& map_x,
        const std::vector<std::vector<int>>& map_y);

    std::map<QRgb, std::vector<QPoint>> extractRegions(const QImage& segmentation);

    QImage warpImage(
        const QImage& src,
        const std::vector<std::vector<QPointF>>& W12);

    QImage dilateScribbles(const QImage& img, int radius);

    QImage overlayScribbles(
        const QImage& base,
        const QImage& scribbles);

    QImage applySegmentationColors(
        const QImage& lineArt,
        const QImage& segmentation);

    std::vector<std::vector<Textoon::UV>> transferUV(
        const std::vector<std::vector<UV>>& prevUV,
        const std::vector<std::vector<QPointF>>& W12,
        const std::map<QRgb, std::vector<QPoint>>& regions);

    QImage render(
        const QImage&                              lineArt,
        const QImage&                              coloredLineArt,
        const std::vector<std::vector<UV>>&        T,
        const std::map<QRgb, std::vector<QPoint>>& regions,
        const QMap<QRgb, QImage>&                  textureMap);
};



#endif // TEXTOON_H
