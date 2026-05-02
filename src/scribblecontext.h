#pragma once

#include <QVector>
#include <QImage>
#include <lazybrush/lazybrush_components/scribble.hpp>
#include <QObject>
#include "lazybrush/lazybrush_include/grid_of_quadtrees_colorizer/colorizer.hpp"
#include "lazybrush/lazybrush_include/grid_of_quadtrees_colorizer/types.hpp"
#include <unordered_set>
//ScribbleContext::ScribbleContext() {}


class ScribbleInfo
{
    using textoons_colorization_context = lazybrush::grid_of_quadtrees_colorizer::colorization_context<ScribbleInfo>;
    using grid_type = typename textoons_colorization_context::working_grid_type;
    using rect_type = typename textoons_colorization_context::rect_type;
    using label_type = typename textoons_colorization_context::label_type;
    using point_type = lazybrush::grid_of_quadtrees_colorizer::point<int>;

public:
    // interface type for internal colorization context

    QImage scrib_image;
    QRect bounding;
    label_type _label{-1};
        // label for identification - this is how you id unique scribbles
        // in our case, labels are indices to palette colors

    /// FOR US
    ScribbleInfo();

    ScribbleInfo(QImage image, QRect rect, label_type label)
        : scrib_image(std::move(image))
        , bounding(rect)
        , _label(label)
    {

        // Initialize contour points
        std::vector<point_type> points;

        if (!scrib_image.isNull())
        {
            for (int y = 1; y < scrib_image.height() - 1; ++y)
            {
                const quint8 * pixel = scrib_image.scanLine(y);
                for (int x = 0; x < scrib_image.width() - 1; ++x, pixel += 4)
                {
                    if (pixel[3] == 0)
                    {
                        continue;
                    }

                    if ((pixel - scrib_image.bytesPerLine() - 4)[3] == 0 ||
                        (pixel - scrib_image.bytesPerLine() - 0)[3] == 0 ||
                        (pixel - scrib_image.bytesPerLine() + 4)[3] == 0 ||
                        (pixel - 4)[3] == 0 ||
                        (pixel + 4)[3] == 0 ||
                        (pixel + scrib_image.bytesPerLine() - 4)[3] == 0 ||
                        (pixel + scrib_image.bytesPerLine() - 0)[3] == 0 ||
                        (pixel + scrib_image.bytesPerLine() + 4)[3] == 0)
                    {
                        points.push_back(point_type(x + bounding.x(), y + bounding.y()));
                    }
                }
            }
        }
    }

    /// FOR THE COLORIZER

    rect_type rect() const {
        return rect_type(bounding.x(), bounding.y(), bounding.width(), bounding.height());
    }

    std::vector<point_type> points;

    bool contains_point(point_type const& p) const
    {
        int local_x = p.x() - bounding.x();
        int local_y = p.y() - bounding.y();

        if (
            local_x < 0 || local_y < 0 ||
            local_x >= scrib_image.width() ||
            local_y >= scrib_image.height()
            )
        {
            return false;
        }

        QColor c = scrib_image.pixelColor(local_x, local_y);

        // Treat any visible pixel as part of the scribble
        return c.alpha() > 0;
    }

    std::vector<point_type> contour_points() const {
        return points;
    }

    label_type label() const {
        return _label;
    }
};

struct SegmentInfo
{
    // interface type for internal colorization context
    QImage image;
    QRect rect;
};

class ScribbleContext : public QObject
{
    using textoons_colorization_context = lazybrush::grid_of_quadtrees_colorizer::colorization_context<ScribbleInfo>;
    using lz_colorization_context = lazybrush::grid_of_quadtrees_colorizer::colorization_context<colorizer_scribble>;
    using grid_type = typename textoons_colorization_context::working_grid_type;
    using rect_type = typename textoons_colorization_context::rect_type;
    using label_type = typename textoons_colorization_context::label_type;
    using point_type = lazybrush::grid_of_quadtrees_colorizer::point<int>;

    Q_OBJECT
private:
    QVector<ScribbleInfo> saved_scribbles;
    QSize _size;
    QImage combined_scribbles;
    QString inputFolder;

public:
    //bool updated;

    std::vector<textoons_colorization_context::input_point> points;

    explicit ScribbleContext(QObject* parent = nullptr, const QString &inputFolder = nullptr)
        : QObject(parent),
        inputFolder(inputFolder)
    {
    }

    //~ScribbleContext();

    const QSize size();
    const QImage getScribblesAsImage();

    QImage colorize(const QVector<ScribbleInfo>& scribbles, const QImage& frame);
    QImage colorize(const QImage& scribbles_image, const QImage& frame);

    void storeSampledPoints(const std::vector<lz_colorization_context::input_point>& sampled_points);

    std::vector<textoons_colorization_context::input_point>
    processPoints(const QImage& original_image_);
    void updateScribbles(const QVector<colorizer_scribble>& scribbles,
                                         const QSize m_size);
    void saveScribblesWithImageSize();
    void saveScribblesWithoutImageSize();

    std::unordered_set<short> collectLabelsFromScribbles();
    std::map<QRgb, short> generateColorToLabelMap();

    QVector<ScribbleInfo> extractScribblesFromQImage(const QImage& scribbles_image);
    QImage createMaskByColor(const QRect& bounding, const QImage& original, const QColor& color);

signals:
    void scribblesUpdated();
};
