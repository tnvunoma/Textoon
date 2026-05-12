#include "postprocessing.h"
#include <Eigen/Sparse>
#include <map>
#include <cmath>
#include <unordered_map>
#include "textoon.h"
#include "iostream"

#include "postprocessing.h"

#include <Eigen/Sparse>
#include <unordered_map>
#include <map>
#include <cmath>
#include <algorithm>

using SpMat = Eigen::SparseMatrix<double>;
using EigenTriplet = Eigen::Triplet<double>;

struct QPointHash
{
    size_t operator()(const QPoint& p) const
    {
        return std::hash<int>()(p.x()) ^
               (std::hash<int>()(p.y()) << 1);
    }
};

std::vector<std::vector<Textoon::UV>>
PostProcessing::textureRounding(
    const std::vector<std::vector<Textoon::UV>>& uv,
    const QImage& segmentation,
    const std::vector<std::vector<QVector3D>>& normals)
{
    int h = uv.size();
    int w = uv[0].size();

    auto result = uv;

    // ------------------------------------------------------------
    // Build regions
    // ------------------------------------------------------------
    std::map<QRgb, std::vector<QPoint>> regions;

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            QRgb c = segmentation.pixel(x, y);

            if (qAlpha(c) == 0)
                continue;

            regions[c].push_back(QPoint(x, y));
        }
    }

    // ------------------------------------------------------------
    // Per-region bulge
    // ------------------------------------------------------------
    for (const auto& [color, pixels] : regions)
    {
        if (pixels.size() < 10)
            continue;

        // --------------------------------------------------------
        // Build boundary mask
        // --------------------------------------------------------
        std::unordered_set<QPoint, QPointHash> boundary;

        const int dx4[4] = {1, -1, 0, 0};
        const int dy4[4] = {0, 0, 1, -1};

        for (const QPoint& p : pixels)
        {
            int x = p.x();
            int y = p.y();

            bool isBoundary = false;

            for (int k = 0; k < 4; ++k)
            {
                int nx = x + dx4[k];
                int ny = y + dy4[k];

                if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                {
                    isBoundary = true;
                    break;
                }

                if (segmentation.pixel(nx, ny) != color)
                {
                    isBoundary = true;
                    break;
                }
            }

            if (isBoundary)
                boundary.insert(p);
        }

        // --------------------------------------------------------
        // Distance-to-boundary field
        // --------------------------------------------------------
        std::unordered_map<QPoint, float, QPointHash> distField;

        float maxDist = 1.f;

        for (const QPoint& p : pixels)
        {
            if (boundary.contains(p))
            {
                distField[p] = 0.f;
                continue;
            }

            float best = 1e9f;

            for (const QPoint& b : boundary)
            {
                float dx = float(p.x() - b.x());
                float dy = float(p.y() - b.y());

                float d = std::sqrt(dx * dx + dy * dy);

                if (d < best)
                    best = d;
            }

            distField[p] = best;
            maxDist = std::max(maxDist, best);
        }

        // --------------------------------------------------------
        // Apply bevel-like deformation
        // --------------------------------------------------------
        const float BULGE_STRENGTH = 0.03f;

        for (const QPoint& p : pixels)
        {
            int x = p.x();
            int y = p.y();

            QVector3D nrm = normals[y][x];

            // normal map [0,1] -> [-1,1]
            float nx = (nrm.x() * 2.f) - 1.f;
            float ny = (nrm.y() * 2.f) - 1.f;

            float mag = std::sqrt(nx * nx + ny * ny);

            if (mag < 1e-5f)
                continue;

            nx /= mag;
            ny /= mag;

            // ----------------------------------------------------
            // Edge weighting
            // 1 at boundary
            // 0 toward center
            // ----------------------------------------------------
            float d = distField[p] / maxDist;

            // smooth bevel falloff
            float edgeWeight =
                std::exp(-4.0f * d);

            float amount =
                BULGE_STRENGTH * edgeWeight;

            // push UVs opposite normal
            result[y][x].u -= nx * amount;
            result[y][x].v -= ny * amount;

            // avoid invalid UVs
            result[y][x].u =
                std::clamp(result[y][x].u, 0.f, 1.f);

            result[y][x].v =
                std::clamp(result[y][x].v, 0.f, 1.f);
        }
    }

    return result;
}

QImage PostProcessing::multiplyShading(
    const QImage& image,
    const QImage& shading)
{
    int w = image.width();
    int h = image.height();

    QImage result(w, h, QImage::Format_ARGB32);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            QColor c(image.pixel(x, y));
            float s = qRed(shading.pixel(x, y)) / 255.f;

            int r = static_cast<int>(c.red()   * s + 0.5f);
            int g = static_cast<int>(c.green() * s + 0.5f);
            int b = static_cast<int>(c.blue()  * s + 0.5f);

            result.setPixel(x, y, qRgba(r, g, b, c.alpha()));
        }
    }

    return result;
}

QImage PostProcessing::lambertianShading(
    const std::vector<std::vector<QVector3D>>& normals,
    const QVector3D& lightDir)
{
    int h = normals.size();
    int w = normals[0].size();

    QVector3D L = lightDir.normalized();

    QImage result(w, h, QImage::Format_ARGB32);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const QVector3D& n = normals[y][x];

            float intensity = 0.f;

            if (n.lengthSquared() > 0.f)
                intensity = std::max(0.f, QVector3D::dotProduct(n, L));

            int val = static_cast<int>(intensity * 255.f + 0.5f);
            result.setPixel(x, y, qRgb(val, val, val));
        }
    }

    return result;
}
