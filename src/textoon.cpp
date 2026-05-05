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
#include <iostream>

struct FrameState
{
    QImage image;
    QImage scribbles;
    QImage segmentation;
    std::vector<std::vector<Textoon::UV>> uv;
};

void Textoon::processFolder(const QString &inputFolder, const QString &anim_name)
{
    outputFolder = inputFolder + "/output";
    anim_nm = anim_name;
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
    if (!f.is_open()) {
        std::cerr << "Failed to load csv file: " + path + ". You need this to run the pipeline!" << std::endl;
        return {};
    }
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
    // ? - match this exactly
    // so ???? = match exactly 4 chars
    filters << anim_nm + "_????.png"
            << anim_nm + "_????.jpg"
            << anim_nm + "_????.bmp";

    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    if (fileList.isEmpty()) return;

    // Load textures from texture/ subfolder
    QDir texDir(inputFolder + "/texture");
    QStringList texFilters;
    texFilters << "*.png" << "*.jpg" << "*.bmp";
    QFileInfoList texFiles = texDir.entryInfoList(texFilters, QDir::Files, QDir::Name);

    // Map from color region -> texture image
    // textures are named by their seg color hex e.g. ff0000.png and must be same size as frame
    QMap<QRgb, QImage> textureMap;
    for (const QFileInfo& f : texFiles)
    {
        bool ok = false;
        uint rgb = f.baseName().toUInt(&ok, 16);
        if (ok)
        {
            QRgb color = rgb | 0xFF000000;
            QImage tex(f.absoluteFilePath());
            if (!tex.isNull())
                textureMap[color] = tex.convertToFormat(QImage::Format_RGB32);
        }
    }

    // -------------------------------------
    // initialize reference frame
    // -------------------------------------
    FrameState prev;

    QImage rawLine0  = QImage(fileList[0].absoluteFilePath());
    prev.image = QImage(fileList[0].absoluteFilePath());

    QString debugDir = inputFolder + "/miscellanea";
    QDir().mkpath(debugDir);

    prev.scribbles = loadInitialScribbles(inputFolder);
    // prev.scribbles = dilateScribbles(prev.scribbles, 2);
    QImage debugScribbles0 = overlayScribbles(rawLine0, prev.scribbles);
    debugScribbles0.save(debugDir + "/scribbles_0000.png");

    // get lazybrush segmentation
    prev.segmentation = scribbleContext->colorize(prev.scribbles, prev.image);
    prev.segmentation.save(debugDir + "/segmentation_0000.png");
    auto region0 = extractRegions(prev.segmentation);

    // Initialize UV (identity)
    prev.uv = initUV(prev.image.width(), prev.image.height());

    // color with segmentation colors
    prev.image = applySegmentationColors(rawLine0, prev.segmentation);

    // Render F0
    QImage rendered0 = textureMap.isEmpty()
                           ? prev.image
                           : textureInitFrame(rawLine0, prev.image, region0, textureMap);
    rendered0.save(outputFolder + "/frame_0000.png");

    // -------------------------------------
    // Process remaining frames
    // -------------------------------------
    for (int i = 1; i < fileList.size(); ++i)
    {
        FrameState curr;

        QImage rawLine = QImage(fileList[i].absoluteFilePath());
        curr.image = QImage(fileList[i].absoluteFilePath());
        QString frameId = QString("%1").arg(i, 4, 10, QChar('0'));

        // Load ARAP map
        auto map_x = loadCsv(inputFolder.toStdString() + "/map_x_" + std::to_string(i) + ".csv");
        auto map_y = loadCsv(inputFolder.toStdString() + "/map_y_" + std::to_string(i) + ".csv");
        if (map_x.empty() || map_y.empty()){
            continue;
        }

        auto W = buildMap(map_x, map_y);

        // Transfer scribbles
        curr.scribbles = warpImage(prev.scribbles, W);
        // curr.scribbles = dilateScribbles(curr.scribbles, 15);
        QImage debugScribbles = overlayScribbles(rawLine, curr.scribbles);
        debugScribbles.save(debugDir + "/scribbles_" + frameId + ".png");

        // LazyBrush segmentation
        curr.segmentation = scribbleContext->colorize(curr.scribbles, curr.image);
        curr.segmentation.save(debugDir + "/segmentation_" + frameId + ".png");
        auto regions = extractRegions(curr.segmentation);

        // color with segmentation colors
        curr.image = applySegmentationColors(rawLine, curr.segmentation);

        // UV transfer
        curr.uv = transferUV(prev.uv, W, regions);

        // Render
        QImage rendered = textureMap.isEmpty()
                              ? curr.image
                              : render(rawLine, curr.image, curr.uv, regions, textureMap);

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

QImage Textoon::textureInitFrame(
    const QImage&                              lineArt,
    const QImage&                              coloredLineArt,
    const std::map<QRgb, std::vector<QPoint>>& regions,
    const QMap<QRgb, QImage>&                  textureMap)
{
    QImage out  = coloredLineArt.convertToFormat(QImage::Format_RGB32);
    QImage line = lineArt.convertToFormat(QImage::Format_RGB32);

    for (const auto& [color, pixels] : regions)
    {
        if (!textureMap.contains(color))
            continue;

        const QImage& tex = textureMap[color];

        for (const QPoint& p : pixels)
        {
            int x = p.x();
            int y = p.y();

            // texture pixel directly maps to frame pixel
            QRgb texPx  = reinterpret_cast<const QRgb*>(tex.scanLine(y))[x];
            QRgb linePx = reinterpret_cast<const QRgb*>(line.scanLine(y))[x];

            int r = (qRed(linePx)   * qRed(texPx))   / 255;
            int g = (qGreen(linePx) * qGreen(texPx)) / 255;
            int b = (qBlue(linePx)  * qBlue(texPx))  / 255;

            reinterpret_cast<QRgb*>(out.scanLine(y))[x] = qRgb(r, g, b);
        }
    }

    return out;
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

// QImage Textoon::dilateScribbles(const QImage& img, int radius)
// {
//     int w = img.width();
//     int h = img.height();
//     QImage out(w, h, QImage::Format_ARGB32);
//     out.fill(Qt::transparent);

//     for (int y = 0; y < h; ++y)
//     {
//         for (int x = 0; x < w; ++x)
//         {
//             for (int dy = -radius; dy <= radius; ++dy)
//             {
//                 for (int dx = -radius; dx <= radius; ++dx)
//                 {
//                     int nx = x + dx;
//                     int ny = y + dy;
//                     if (nx < 0 || nx >= w || ny < 0 || ny >= h)
//                         continue;

//                     QColor c(img.pixel(nx, ny));
//                     if (c.alpha() == 0) continue;

//                     out.setPixelColor(x, y, c);
//                     goto next_pixel;
//                 }
//             }
//         next_pixel:;
//         }
//     }
//     return out;
// }

QImage Textoon::overlayScribbles(
    const QImage& base,
    const QImage& scribbles)
{
    int w = base.width();
    int h = base.height();

    QImage result = base.convertToFormat(QImage::Format_ARGB32);
    QImage scrib = scribbles.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < h; ++y)
    {
        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(scrib.scanLine(y));

        for (int x = 0; x < w; ++x)
        {
            QRgb s = srcLine[x];

            int a = qAlpha(s);
            int r = qRed(s);
            int g = qGreen(s);
            int b = qBlue(s);

            if (a == 0) continue;
            if (r == 0 && g == 0 && b == 0) continue;

            dstLine[x] = s;
        }
    }

    return result;
}


QImage Textoon::applySegmentationColors(
    const QImage& lineArt,
    const QImage& segmentation)
{
    int w = lineArt.width();
    int h = lineArt.height();
    QImage out(w, h, QImage::Format_ARGB32);

    QImage line = lineArt.convertToFormat(QImage::Format_ARGB32);
    QImage seg  = segmentation.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < h; ++y)
    {
        const QRgb* lineRow = (const QRgb*)line.scanLine(y);
        const QRgb* segRow  = (const QRgb*)seg.scanLine(y);
        QRgb*       outRow  = (QRgb*)out.scanLine(y);

        for (int x = 0; x < w; ++x)
        {
            QRgb linePx = lineRow[x];
            QRgb segPx  = segRow[x];

            bool hasSeg = qAlpha(segPx) > 10;

            if (!hasSeg)
            {
                // No seg region, just show base lineart img
                outRow[x] = linePx;
            }
            else
            {
                // Multiply blend
                int r = (qRed(linePx)   * qRed(segPx))   / 255;
                int g = (qGreen(linePx) * qGreen(segPx)) / 255;
                int b = (qBlue(linePx)  * qBlue(segPx))  / 255;

                outRow[x] = qRgba(r, g, b, 255);
            }
        }
    }
    return out;
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
    // Forward warp with bilinear splatting
    // ---------------------------------
    std::vector<std::vector<float>> accumU(h, std::vector<float>(w, 0.f));
    std::vector<std::vector<float>> accumV(h, std::vector<float>(w, 0.f));
    std::vector<std::vector<float>> accumW(h, std::vector<float>(w, 0.f));

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            QPointF qf = W12[y][x];
            int x0 = (int)std::floor(qf.x());
            int y0 = (int)std::floor(qf.y());
            float fx = qf.x() - x0;
            float fy = qf.y() - y0;

            float w00 = (1.f - fx) * (1.f - fy);
            float w10 = fx           * (1.f - fy);
            float w01 = (1.f - fx) * fy;
            float w11 = fx           * fy;

            auto splat = [&](int nx, int ny, float wt)
            {
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) return;
                accumU[ny][nx] += prevUV[y][x].u * wt;
                accumV[ny][nx] += prevUV[y][x].v * wt;
                accumW[ny][nx] += wt;
            };
            splat(x0,     y0,     w00);
            splat(x0 + 1, y0,     w10);
            splat(x0,     y0 + 1, w01);
            splat(x0 + 1, y0 + 1, w11);
        }
    }

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (accumW[y][x] > 0.f)
            {
                T2[y][x] = { accumU[y][x] / accumW[y][x],
                            accumV[y][x] / accumW[y][x] };
                assigned[y][x] = true;
            }
        }
    }

    // ---------------------------------
    // Process each region
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
        const int MAX_CTRL = 80;
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
    const QImage&                              lineArt,
    const QImage&                              coloredLineArt,  // output of applySegmentationColors
    const std::vector<std::vector<UV>>&        T,
    const std::map<QRgb, std::vector<QPoint>>& regions,
    const QMap<QRgb, QImage>&                  textureMap)
{
    int w = lineArt.width();
    int h = lineArt.height();

    QImage out  = coloredLineArt.convertToFormat(QImage::Format_RGB32);
    QImage line = lineArt.convertToFormat(QImage::Format_RGB32);

    for (const auto& [color, pixels] : regions)
    {
        if (!textureMap.contains(color))
            continue; // no texture for this region, flat color stays

        const QImage& tex = textureMap[color]; // same size as frame, Format_RGB32

        for (const QPoint& p : pixels)
        {
            int x = p.x();
            int y = p.y();

            float u = std::clamp(T[y][x].u, 0.f, 1.f);
            float v = std::clamp(T[y][x].v, 0.f, 1.f);

            int tx = int(u * (tex.width()  - 1));
            int ty = int(v * (tex.height() - 1));

            QRgb texPx  = reinterpret_cast<const QRgb*>(tex.scanLine(ty))[tx];
            QRgb linePx = reinterpret_cast<const QRgb*>(line.scanLine(y))[x];

            // Multiply: texture color under lineart, lines darken through
            int r = (qRed(linePx)   * qRed(texPx))   / 255;
            int g = (qGreen(linePx) * qGreen(texPx)) / 255;
            int b = (qBlue(linePx)  * qBlue(texPx))  / 255;

            reinterpret_cast<QRgb*>(out.scanLine(y))[x] = qRgb(r, g, b);
        }
    }

    return out;
}
