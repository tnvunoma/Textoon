#ifndef POSTPROCESSING_H
#define POSTPROCESSING_H
#pragma once
#include <QImage>
#include <QVector3D>
#include <vector>
#include "textoon.h"

class PostProcessing
{
public:

    static std::vector<std::vector<Textoon::UV>>
    textureRounding(
        const std::vector<std::vector<Textoon::UV>>& uv,
        const QImage& segmentation,
        const std::vector<std::vector<QVector3D>>& normals);

    static QImage lambertianShading(
        const std::vector<std::vector<QVector3D>>& normals,
        const QVector3D& lightDir);

    static QImage multiplyShading(
        const QImage& image,
        const QImage& shading);

};

#endif // POSTPROCESSING_H
