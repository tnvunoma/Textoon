#include "textoon.h"

#include <QDir>
#include <QFileInfoList>
#include <QImage>
#include <QDebug>
#include <QColor>
#include <queue>
#include <QPoint>

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

QImage Textoon::transferUV(const QImage &F1)
{
    int w = F1.width();
    int h = F1.height();

    QImage F2(w, h, QImage::Format_RGB32);
    F2.fill(Qt::black);

    // Convert F1 to RGB32 for consistent scanLine access
    QImage src = F1.convertToFormat(QImage::Format_RGB32);

    std::vector<std::vector<UV>> T2(h, std::vector<UV>(w, {0.f, 0.f}));
    std::vector<std::vector<bool>> assigned(h, std::vector<bool>(w, false));

    // Forward warp
    float angle = 20.0f * M_PI / 180.0f;
    float cx = w / 2.0f;
    float cy = h / 2.0f;

    auto rotate = [&](float x, float y) {
        float dx = x - cx, dy = y - cy;
        return QPointF(cos(angle)*dx - sin(angle)*dy + cx,
                       sin(angle)*dx + cos(angle)*dy + cy);
    };

    std::vector<std::vector<UV>> T1(h, std::vector<UV>(w));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            T1[y][x] = { float(x)/w, float(y)/h };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if ((x + y) % 5 == 0) continue; // artificial holes

            QPointF qf = rotate(x, y);
            int qx = std::round(qf.x());
            int qy = std::round(qf.y());

            if (qx >= 0 && qx < w && qy >= 0 && qy < h)
            {
                if (!assigned[qy][qx]) {
                    T2[qy][qx] = T1[y][x];
                    assigned[qy][qx] = true;
                } else {
                    T2[qy][qx].u = 0.5f*(T2[qy][qx].u + T1[y][x].u);
                    T2[qy][qx].v = 0.5f*(T2[qy][qx].v + T1[y][x].v);
                }
            }
        }
    }

    // Collect known/unknown
    std::vector<QPoint> known, unknown;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            (assigned[y][x] ? known : unknown).emplace_back(x, y);

    // Save pre-interpolation image
    {
        QImage preInterp(w, h, QImage::Format_RGB32);
        preInterp.fill(Qt::black); // black = missing pixels

        for (int y = 0; y < h; ++y)
        {
            QRgb* outLine = reinterpret_cast<QRgb*>(preInterp.scanLine(y));
            const QRgb* srcLine_base = reinterpret_cast<const QRgb*>(src.scanLine(0));

            for (int x = 0; x < w; ++x)
            {
                if (!assigned[y][x]) continue; // leave black

                float u = std::clamp(T2[y][x].u, 0.f, 1.f);
                float v = std::clamp(T2[y][x].v, 0.f, 1.f);
                int sx = int(u * (w - 1));
                int sy = int(v * (h - 1));
                const QRgb* srcLine = reinterpret_cast<const QRgb*>(src.scanLine(sy));
                outLine[x] = srcLine[sx];
            }
        }

        QString preInterpPath = outputFolder + "/pre_interp_debug.png";
        preInterp.save(preInterpPath);
    }

    // Subsample control points so runtime doesn;t explode
    // Keep at 60 points for now: O(N^3) solve + O(N * W * H) BFS evaluation
    const int MAX_CTRL = 60;
    std::vector<QPoint> sampled;
    {
        int step = std::max(1, (int)known.size() / MAX_CTRL);
        for (int i = 0; i < (int)known.size() && (int)sampled.size() < MAX_CTRL; i += step)
            sampled.push_back(known[i]);
    }

    qDebug() << "Control points:" << sampled.size()
             << "  Unknown:" << unknown.size();

    // One BFS map per control point
    std::vector<std::vector<bool>> mask(h, std::vector<bool>(w, true));
    std::vector<std::vector<std::vector<float>>> allDist;
    allDist.reserve(sampled.size());
    for (const auto& seed : sampled)
        allDist.push_back(computeDistanceMap(w, h, mask, seed));

    // Build value vectors
    std::vector<double> values_u, values_v;
    for (const auto& p : sampled) {
        values_u.push_back(T2[p.y()][p.x()].u);
        values_v.push_back(T2[p.y()][p.x()].v);
    }

    // Fit TPS
    TPS tps_u = fitTPS(sampled, values_u, allDist);
    TPS tps_v = fitTPS(sampled, values_v, allDist);

    // Fill unknown pixels
    for (const auto& p : unknown)
    {
        T2[p.y()][p.x()].u = evaluateTPS(tps_u, p, allDist);
        T2[p.y()][p.x()].v = evaluateTPS(tps_v, p, allDist);
    }

    // Render via scanLine
    for (int y = 0; y < h; ++y)
    {
        QRgb* outLine = reinterpret_cast<QRgb*>(F2.scanLine(y));
        for (int x = 0; x < w; ++x)
        {
            float u = std::clamp(T2[y][x].u, 0.f, 1.f);
            float v = std::clamp(T2[y][x].v, 0.f, 1.f);
            int sx = int(u * (w - 1));
            int sy = int(v * (h - 1));
            const QRgb* srcLine = reinterpret_cast<const QRgb*>(src.scanLine(sy));
            outLine[x] = srcLine[sx];
        }
    }

    return F2;
}
