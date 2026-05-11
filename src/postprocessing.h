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

};

#endif // POSTPROCESSING_H
