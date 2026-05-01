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

        // for (int y = 0; y < scrib_image.height(); y++)
        // {
        //     for (int x = 0; x < scrib_image.width(); x++)
        //     {
        //         QColor c = scrib_image.pixelColor(x, y);

        //         if (c.alpha() > 0)
        //         {
        //             points.emplace_back(rect.x() + x, rect.y() + y);
        //         }
        //     }
        // }
    }

    rect_type rect() const {
        return rect_type(bounding.x(), bounding.y(), bounding.width(), bounding.height());
    }

    std::vector<point_type> points;

    /// FOR THE COLORIZER
    /*
    scribble::contour_points() const
    {
        if (!cache_is_valid_)
        {
            cached_contour_points_.clear();

            if (!image_.isNull())
            {
                for (int y = 1; y < image_.height() - 1; ++y)
                {
                    const quint8 * pixel = image_.scanLine(y);
                    for (int x = 0; x < image_.width() - 1; ++x, pixel += 4)
                    {
                        if (pixel[3] == 0)
                        {
                            continue;
                        }

                        if ((pixel - image_.bytesPerLine() - 4)[3] == 0 ||
                            (pixel - image_.bytesPerLine() - 0)[3] == 0 ||
                            (pixel - image_.bytesPerLine() + 4)[3] == 0 ||
                            (pixel - 4)[3] == 0 ||
                            (pixel + 4)[3] == 0 ||
                            (pixel + image_.bytesPerLine() - 4)[3] == 0 ||
                            (pixel + image_.bytesPerLine() - 0)[3] == 0 ||
                            (pixel + image_.bytesPerLine() + 4)[3] == 0)
                        {
                            points.push_back(point_type(x + image_rect_.x(), y + image_rect_.y()));
                        }
                    }
                }
            }

            cache_is_valid_ = true;
        }
        return cached_contour_points_;
    }*/

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

public:

    std::vector<textoons_colorization_context::input_point> points;


    explicit ScribbleContext(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    //~ScribbleContext();

    //const
    const QSize size();
    const QImage getScribblesAsImage();

    QImage colorize(const QVector<ScribbleInfo>& scribbles);
    QImage colorize(const QImage& scribbles_image);

    void storeSampledPoints(const std::vector<lz_colorization_context::input_point>& sampled_points);
    void storeScribbles(const QVector<colorizer_scribble>& scribbles,
                                         const QSize m_size);
    void saveScribblesWithImageSize();
    void saveScribblesWithoutImageSize();

    std::unordered_set<short> collectLabelsFromScribbles();
    std::map<QRgb, short> generateColorToLabelMap();

    QVector<ScribbleInfo> extractScribblesFromQImage(const QImage& scribbles_image);
    QImage createMaskByColor(const QRect& bounding, const QImage& original, const QColor& color);
};

//extern ScribbleContext scribble_context;

