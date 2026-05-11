#include "postprocessing.h"
#include <Eigen/Sparse>
#include <map>
#include <cmath>
#include <unordered_map>
#include "textoon.h"


using SpMat = Eigen::SparseMatrix<double>;
using EigenTriplet = Eigen::Triplet<double>;
struct QPointHash {
    size_t operator()(const QPoint& p) const {
        return std::hash<int>()(p.x()) ^ (std::hash<int>()(p.y()) << 1);
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

    // ---------------------------------
    // Build regions from segmentation
    // ---------------------------------
    std::map<QRgb, std::vector<QPoint>> regions;

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            QRgb c = segmentation.pixel(x, y);
            if (c == qRgb(0, 0, 0))
                continue; // skip background if needed
            regions[c].push_back(QPoint(x, y));
        }
    }

    // ---------------------------------
    // 2. Solve per region
    // ---------------------------------
    for (const auto &[color, pixels] : regions)
    {
        int n = pixels.size();
        if (n < 10)
            continue;

        // ---------------------------------
        // Compute region centroid
        // ---------------------------------
        float cx = 0.f;
        float cy = 0.f;

        for (const QPoint& p : pixels)
        {
            cx += p.x();
            cy += p.y();
        }

        cx /= float(n);
        cy /= float(n);

        // ---------------------------------
        // Compute max radius
        // ---------------------------------
        float maxDist = 1.f;

        for (const QPoint& p : pixels)
        {
            float dx = p.x() - cx;
            float dy = p.y() - cy;

            float d = std::sqrt(dx * dx + dy * dy);

            maxDist = std::max(maxDist, d);
        }

        // ---------------------------------
        // Apply bulge deformation
        // ---------------------------------
        const float BULGE_STRENGTH = 0.03f;

        for (const QPoint& p : pixels)
        {
            int x = p.x();
            int y = p.y();

            float dx = x - cx;
            float dy = y - cy;

            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < 1e-5f)
                continue;

            // normalized radial direction
            dx /= dist;
            dy /= dist;

            // paraboloid falloff
            float t = dist / maxDist;

            float bulge =
                BULGE_STRENGTH * (1.f - t * t);

            // normal influence
            QVector3D nrm = normals[y][x];

            float normalScale =
                std::sqrt(
                    nrm.x() * nrm.x() +
                    nrm.y() * nrm.y());

            bulge *= (0.5f + normalScale);

            result[y][x].u += dx * bulge;
            result[y][x].v += dy * bulge;
        }

        // map pixel -> local index
        std::unordered_map<QPoint, int, QPointHash> idx;
        for (int i = 0; i < n; ++i)
            idx[pixels[i]] = i;

        std::vector<EigenTriplet> triplets;
        Eigen::VectorXd bu(n), bv(n);
        bu.setZero();
        bv.setZero();

        for (int i = 0; i < n; ++i)
        {
            int x = pixels[i].x();
            int y = pixels[i].y();

            double diag = 0.0;
            bool isBoundary = false;

            const int dx[4] = {1, -1, 0, 0};
            const int dy[4] = {0, 0, 1, -1};

            for (int k = 0; k < 4; ++k)
            {
                int nx = x + dx[k];
                int ny = y + dy[k];

                if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                    continue;

                if (segmentation.pixel(nx, ny) != color)
                {
                    isBoundary = true;
                    continue;
                }

                int j = idx[QPoint(nx, ny)];

                // weight from normals
                double hi = std::sqrt(
                    normals[y][x].x() * normals[y][x].x() +
                    normals[y][x].y() * normals[y][x].y());

                double hj = std::sqrt(
                    normals[ny][nx].x() * normals[ny][nx].x() +
                    normals[ny][nx].y() * normals[ny][nx].y());

                double diff = hi - hj;
                double w_ij = 1.0 / std::sqrt(1.0 + diff * diff);

                triplets.emplace_back(i, j, -w_ij);
                diag += w_ij;
            }

            if (isBoundary)
            {
                // Dirichlet constraint
                triplets.emplace_back(i, i, 1.0);
                bu[i] = result[y][x].u;
                bv[i] = result[y][x].v;
            }
            else
            {
                triplets.emplace_back(i, i, diag);
            }
        }

        // ---------------------------------
        // Solve region
        // ---------------------------------
        SpMat L(n, n);
        L.setFromTriplets(triplets.begin(), triplets.end());

        Eigen::SimplicialLDLT<SpMat> solver;
        solver.compute(L);

        if (solver.info() != Eigen::Success)
            continue;

        Eigen::VectorXd fu = solver.solve(bu);
        Eigen::VectorXd fv = solver.solve(bv);

        if (solver.info() != Eigen::Success)
            continue;

        // ---------------------------------
        // Write back
        // ---------------------------------
        for (int i = 0; i < n; ++i)
        {
            int x = pixels[i].x();
            int y = pixels[i].y();

            result[y][x].u = fu[i];
            result[y][x].v = fv[i];
        }
    }

    return result;
}
