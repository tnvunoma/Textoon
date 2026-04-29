#pragma once

#include <QVector>
#include <QImage>
#include <lazybrush/lazybrush_components/scribble.hpp>
#include <QObject>
#include "lazybrush/lazybrush_include/grid_of_quadtrees_colorizer/colorizer.hpp"
#include "lazybrush/lazybrush_include/grid_of_quadtrees_colorizer/types.hpp"
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

    QImage original_image;
    QRect qrect;
    label_type _label{-1};
        // label for identification - this is how you id unique scribbles

    /// FOR US
    ScribbleInfo();

    ScribbleInfo(QImage image, QRect rect, label_type label)
        : original_image(std::move(image))
        , qrect(rect)
        , _label(label)
    {

        // Initialize contour points
        std::vector<point_type> points;

        for (int y = 0; y < original_image.height(); ++y)
        {
            for (int x = 0; x < original_image.width(); ++x)
            {
                QColor c = original_image.pixelColor(x, y);

                if (c.alpha() > 0)
                {
                    points.emplace_back(rect.x() + x, rect.y() + y);
                }
            }
        }
    }

    rect_type rect() const {
        return rect_type(qrect.x(), qrect.y(), qrect.width(), qrect.height());
    }

    std::vector<point_type> points;

    /// FOR THE COLORIZER

    bool contains_point(point_type const& p) const
    {
        int local_x = p.x() - qrect.x();
        int local_y = p.y() - qrect.y();

        if (
            local_x < 0 || local_y < 0 ||
            local_x >= original_image.width() ||
            local_y >= original_image.height()
            )
        {
            return false;
        }

        QColor c = original_image.pixelColor(local_x, local_y);

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

public:
    QVector<ScribbleInfo> saved_scribbles;
    QSize size;
    QImage combined_scribbles;
    std::vector<textoons_colorization_context::input_point> points;


    explicit ScribbleContext(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    //~ScribbleContext();

    QImage colorize(const ScribbleInfo& scribble);
    void storeSampledPoints(const std::vector<lz_colorization_context::input_point> sampled_points);
    void storeScribbles(const QVector<colorizer_scribble>& scribbles,
                                         const QSize m_size);
    ScribbleInfo createScribblesFromQImage(QImage image, label_type label);
    void saveScribblesWithImageSize();
    void saveScribblesWithoutImageSize();

    QImage imgColorize(const QImage& scribbles);

};

//extern ScribbleContext scribble_context;

