#include "textoon.h"

#include <QDir>
#include <QFileInfoList>
#include <QImage>
#include <QDebug>
#include <QColor>
#include <queue>
#include <QPoint>
#include <fstream>
#include "scribblecontext.h"

Textoon::Textoon()
{
}

struct FrameState
{
    QImage image;
    QImage scribbles;
    QImage segmentation;
    std::vector<std::vector<Textoon::UV>> uv;
};

void Textoon::processFolder(const QString &inputFolder)
{
    outputFolder = inputFolder + "/output";
    QDir().mkpath(outputFolder);

    processFrames(inputFolder);
}

// MAKE SURE TO SAVE SCRIBBLES PNG INSIDE INPUT FOLDER
QImage Textoon::loadInitialScribbles(const QString& inputFolder)
{
    QString path = inputFolder + "/scribbles.png";

    QImage img(path);

    if (img.isNull())
        qDebug() << "Failed to load scribbles:" << path;

    return img;
}

static std::vector<std::vector<int>> loadCsv(const std::string &path)
{
    std::vector<std::vector<int>> data;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty())
            continue;
        std::vector<int> row;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ','))
        {
            if (!cell.empty())
                row.push_back(std::stoi(cell));
        }
        if (!row.empty())
            data.push_back(std::move(row));
    }
    return data;
}

void Textoon::processFrames(const QString &inputFolder)
{
    QDir dir(inputFolder);

    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.bmp";

    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    if (fileList.isEmpty()) return;

    // -------------------------------------
    // initialize reference frame
    // -------------------------------------
    FrameState prev;

    prev.image = QImage(fileList[0].absoluteFilePath());

    prev.scribbles = loadInitialScribbles(inputFolder);

    // get lazybrush segmentation
    prev.segmentation = scribbleContext.imgColorize(prev.scribbles);


    // Initialize UV (identity)
    prev.uv = initUV(prev.image.width(), prev.image.height());

    // Render F0
    QImage rendered0 = render(prev.image, prev.uv);
    rendered0.save(outputFolder + "/frame_0000.png");

    // -------------------------------------
    // Process remaining frames
    // -------------------------------------
    for (int i = 1; i < fileList.size(); ++i)
    {
        FrameState curr;

        curr.image = QImage(fileList[i].absoluteFilePath());

        // Load ARAP map
        auto map_x = loadCsv("map_x_" + std::to_string(i) + ".csv");
        auto map_y = loadCsv("map_y_" + std::to_string(i) + ".csv");

        auto W = buildMap(map_x, map_y);

        // Transfer scribbles
        curr.scribbles = warpImage(prev.scribbles, W);

        // LazyBrush segmentation
        curr.segmentation = scribbleContext.imgColorize(curr.scribbles);

        // extract regions
        auto regions = extractRegions(curr.segmentation);

        // UV transfer
        curr.uv = transferUV(prev.uv, W, regions);

        // Render
        QImage rendered = render(curr.image, curr.uv);

        QString out = outputFolder + QString("/frame_%1.png").arg(i, 4, 10, QChar('0'));
        rendered.save(out);

        prev = curr;
    }

    qDebug() << "Done.";
}


std::vector<std::vector<Textoon::UV>> Textoon::initUV(int w, int h)
{
    std::vector<std::vector<UV>> T(h, std::vector<UV>(w));

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            T[y][x] = { float(x) / w, float(y) / h };

    return T;
}

std::vector<std::vector<QPointF>>
Textoon::buildMap(
    const std::vector<std::vector<int>>& map_x,
    const std::vector<std::vector<int>>& map_y)
{
    int h = map_x.size();
    int w = map_x[0].size();

    std::vector<std::vector<QPointF>> W(h, std::vector<QPointF>(w));

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            W[y][x] = QPointF(map_x[y][x], map_y[y][x]);
        }
    }

    return W;
}

std::map<QRgb, std::vector<QPoint>>
Textoon::extractRegions(const QImage& segmentation)
{
    int w = segmentation.width();
    int h = segmentation.height();

    std::map<QRgb, std::vector<QPoint>> regions;

    QImage seg = segmentation.convertToFormat(QImage::Format_RGB32);

    for (int y = 0; y < h; ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(seg.scanLine(y));

        for (int x = 0; x < w; ++x)
        {
            QRgb color = line[x];

            if (color == qRgb(0,0,0)) continue;

            regions[color].emplace_back(x, y);
        }
    }

    return regions;
}

std::vector<std::vector<float>> computeDistanceMap(
    int w, int h,
    const std::vector<std::vector<bool>>& mask,
    QPoint seed)
{
    std::vector<std::vector<float>> dist(h, std::vector<float>(w, 1e9));

    std::queue<QPoint> q;
    q.push(seed);
    dist[seed.y()][seed.x()] = 0;

    int dx[4] = {1,-1,0,0};
    int dy[4] = {0,0,1,-1};

    while (!q.empty())
    {
        QPoint p = q.front(); q.pop();

        for (int i = 0; i < 4; ++i)
        {
            int nx = p.x() + dx[i];
            int ny = p.y() + dy[i];

            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            if (!mask[ny][nx]) continue;

            float nd = dist[p.y()][p.x()] + 1;

            if (nd < dist[ny][nx])
            {
                dist[ny][nx] = nd;
                q.push(QPoint(nx, ny));
            }
        }
    }

    return dist;
}

double phi(double r)
{
    if (r == 0.0) return 0.0;
    return r * r * std::log(r + 1e-6);
}

struct TPS
{
    VectorXd w;
    Vector3d a;
    std::vector<QPoint> pts;
};

// Returns one BFS distance map per control point
static std::vector<std::vector<std::vector<float>>> computeAllDistanceMaps(
    int w, int h,
    const std::vector<std::vector<bool>>& mask,
    const std::vector<QPoint>& seeds)
{
    std::vector<std::vector<std::vector<float>>> allMaps;
    allMaps.reserve(seeds.size());
    for (const auto& seed : seeds)
        allMaps.push_back(computeDistanceMap(w, h, mask, seed));
    return allMaps;
}

// allDist[i][y][x] = geodesic dist from pts[i] to (x,y)
TPS fitTPS(
    const std::vector<QPoint>& pts,
    const std::vector<double>& values,
    const std::vector<std::vector<std::vector<float>>>& allDist)
{
    int N = pts.size();
    MatrixXd K(N, N);
    MatrixXd P(N, 3);

    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < N; ++j)
        {
            // geodesic distance from pts[i] to pts[j], using BFS map seeded at pts[i]
            float r = allDist[i][pts[j].y()][pts[j].x()];
            K(i, j) = phi(r);
        }
        P(i, 0) = 1;
        P(i, 1) = pts[i].x();
        P(i, 2) = pts[i].y();
    }

    MatrixXd A(N + 3, N + 3);
    A << K, P,
        P.transpose(), MatrixXd::Zero(3, 3);

    VectorXd b(N + 3);
    for (int i = 0; i < N; ++i) b[i] = values[i];
    b.tail(3).setZero();

    VectorXd sol = A.colPivHouseholderQr().solve(b);

    TPS tps;
    tps.w = sol.head(N);
    tps.a = sol.tail(3);
    tps.pts = pts;
    return tps;
}

double evaluateTPS(
    const TPS& tps,
    QPoint x,
    const std::vector<std::vector<std::vector<float>>>& allDist)
{
    double sum = 0.0;
    for (int i = 0; i < (int)tps.pts.size(); ++i)
    {
        // allDist[i] is BFS map seeded at pts[i], so allDist[i][x.y()][x.x()]
        // gives geodesic distance from pts[i] to x
        float r = allDist[i][x.y()][x.x()];
        sum += tps.w[i] * phi(r);
    }
    return sum + tps.a[0] + tps.a[1] * x.x() + tps.a[2] * x.y();
}

QImage Textoon::warpImage(
    const QImage& src,
    const std::vector<std::vector<QPointF>>& W12)
{
    int w = src.width();
    int h = src.height();

    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    std::vector<std::vector<bool>> assigned(h, std::vector<bool>(w, false));

    QImage src32 = src.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < h; ++y)
    {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(src32.scanLine(y));

        for (int x = 0; x < w; ++x)
        {
            QPointF qf = W12[y][x];

            int qx = std::round(qf.x());
            int qy = std::round(qf.y());

            if (qx < 0 || qx >= w || qy < 0 || qy >= h) continue;

            QRgb* dstLine = reinterpret_cast<QRgb*>(dst.scanLine(qy));

            if (!assigned[qy][qx])
            {
                dstLine[qx] = srcLine[x];
                assigned[qy][qx] = true;
            }
            else
            {
                // simple overwrite OR blend
                dstLine[qx] = srcLine[x];
            }
        }
    }

    return dst;
}

std::vector<std::vector<Textoon::UV>>
Textoon::transferUV(
    const std::vector<std::vector<UV>>& prevUV,
    const std::vector<std::vector<QPointF>>& W12,
    const std::map<QRgb, std::vector<QPoint>>& regions)
{
    int h = prevUV.size();
    int w = prevUV[0].size();

    std::vector<std::vector<UV>> T2(h, std::vector<UV>(w, {0.f, 0.f}));
    std::vector<std::vector<bool>> assigned(h, std::vector<bool>(w, false));

    // ---------------------------------
    // Step 1: Forward warp
    // ---------------------------------
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            QPointF qf = W12[y][x];

            int qx = std::round(qf.x());
            int qy = std::round(qf.y());

            if (qx < 0 || qx >= w || qy < 0 || qy >= h)
                continue;

            if (!assigned[qy][qx])
            {
                T2[qy][qx] = prevUV[y][x];
                assigned[qy][qx] = true;
            }
            else
            {
                T2[qy][qx].u = 0.5f * (T2[qy][qx].u + prevUV[y][x].u);
                T2[qy][qx].v = 0.5f * (T2[qy][qx].v + prevUV[y][x].v);
            }
        }
    }

    // ---------------------------------
    // Step 2: Process each region
    // ---------------------------------
    for (const auto& [color, pixels] : regions)
    {
        std::vector<QPoint> known;
        std::vector<QPoint> unknown;

        for (const QPoint& p : pixels)
        {
            if (assigned[p.y()][p.x()])
                known.push_back(p);
            else
                unknown.push_back(p);
        }

        if (known.size() < 5 || unknown.empty())
            continue;

        // ---------------------------------
        // Subsample control points
        // ---------------------------------
        const int MAX_CTRL = 40;
        std::vector<QPoint> sampled;

        int step = std::max(1, (int)known.size() / MAX_CTRL);

        for (int i = 0;
             i < (int)known.size() && (int)sampled.size() < MAX_CTRL;
             i += step)
        {
            sampled.push_back(known[i]);
        }

        // ---------------------------------
        // Build region mask
        // ---------------------------------
        std::vector<std::vector<bool>> mask(h, std::vector<bool>(w, false));

        for (const QPoint& p : pixels)
            mask[p.y()][p.x()] = true;

        // ---------------------------------
        // Distance maps
        // ---------------------------------
        std::vector<std::vector<std::vector<float>>> allDist;

        for (const auto& p : sampled)
            allDist.push_back(computeDistanceMap(w, h, mask, p));

        // ---------------------------------
        // TPS fit
        // ---------------------------------
        std::vector<double> values_u, values_v;

        for (const auto& p : sampled)
        {
            values_u.push_back(T2[p.y()][p.x()].u);
            values_v.push_back(T2[p.y()][p.x()].v);
        }

        TPS tps_u = fitTPS(sampled, values_u, allDist);
        TPS tps_v = fitTPS(sampled, values_v, allDist);

        // ---------------------------------
        // Fill unknown pixels in region
        // ---------------------------------
        for (const auto& p : unknown)
        {
            T2[p.y()][p.x()].u = evaluateTPS(tps_u, p, allDist);
            T2[p.y()][p.x()].v = evaluateTPS(tps_v, p, allDist);
        }
    }

    return T2;
}

QImage Textoon::render(
    const QImage& sourceTexture,
    const std::vector<std::vector<UV>>& T)
{
    int w = sourceTexture.width();
    int h = sourceTexture.height();

    QImage src = sourceTexture.convertToFormat(QImage::Format_RGB32);

    QImage out(w, h, QImage::Format_RGB32);

    for (int y = 0; y < h; ++y)
    {
        QRgb* outLine = reinterpret_cast<QRgb*>(out.scanLine(y));

        for (int x = 0; x < w; ++x)
        {
            float u = std::clamp(T[y][x].u, 0.f, 1.f);
            float v = std::clamp(T[y][x].v, 0.f, 1.f);

            int sx = int(u * (w - 1));
            int sy = int(v * (h - 1));

            const QRgb* srcLine = reinterpret_cast<const QRgb*>(src.scanLine(sy));
            outLine[x] = srcLine[sx];
        }
    }

    return out;
}
