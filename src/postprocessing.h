#ifndef POSTPROCESSING_H
#define POSTPROCESSING_H
#pragma once
#include <QImage>
#include <QVector3D>
#include <vector>

class PostProcessing
{
public:
    struct UV
    {
        float u, v;
    };

    static std::vector<std::vector<UV>> textureRounding(
        const std::vector<std::vector<UV>> &uv,
        const QImage &segmentation,
        const std::vector<std::vector<QVector3D>> &normals);
};

#endif // POSTPROCESSING_H
