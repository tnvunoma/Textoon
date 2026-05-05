#include "postprocessing.h"
#include <Eigen/Sparse>
#include <map>
#include <cmath>
#include <unordered_map>


using SpMat = Eigen::SparseMatrix<double>;
using Triplet = Eigen::Triplet<double>;
struct QPointHash {
    size_t operator()(const QPoint& p) const {
        return std::hash<int>()(p.x()) ^ (std::hash<int>()(p.y()) << 1);
    }
};

std::vector<std::vector<PostProcessing::UV>>
PostProcessing::textureRounding(
    const std::vector<std::vector<UV>> &uv,
    const QImage &segmentation,
    const std::vector<std::vector<QVector3D>> &normals)
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

        // map pixel -> local index
        std::unordered_map<QPoint, int, QPointHash> idx;
        for (int i = 0; i < n; ++i)
            idx[pixels[i]] = i;

        std::vector<Triplet> triplets;
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
                bu[i] = uv[y][x].u;
                bv[i] = uv[y][x].v;
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
