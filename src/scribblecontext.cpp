#include "scribblecontext.h"
#include <QImage>
#include <QPainter>
#include <iostream>
#include <QDir>
#include "lazybrush/lazybrush_components/window.h"



void ScribbleContext::storeSampledPoints(const std::vector<lz_colorization_context::input_point> sampled_points){

    std::transform(sampled_points.begin(),
                   sampled_points.end(),
                   std::back_inserter(points),
                   [=](const lz_colorization_context::input_point& ip)
                   {
                       return textoons_colorization_context::input_point{ip.position, ip.intensity};
                    }
                   );
}

void ScribbleContext::storeScribbles(const QVector<colorizer_scribble>& scribbles,
                                     const QSize m_size)
{
    // Stores given scribbles. Optional argument m_size indicates the size of an original image
    // to save with respect to
    saved_scribbles.clear();

    for (const colorizer_scribble& cs : scribbles)
    {
        scribble s = cs.getScribble();
        saved_scribbles.push_back({s.image(), s.rect(), cs.label()});
    }

    size = m_size;
}

void ScribbleContext::saveScribblesWithImageSize()
{
    // Saves all scribbles to one combined image; size of output image is with
    // respect to the original image, if one was loaded

    const QVector<ScribbleInfo>& scribbles = saved_scribbles;

    if (scribbles.empty()) {
        std::cerr << "No scribbles to save." << std::endl;
        return;
    }

    // Create output image
    QImage combined(size, QImage::Format_ARGB32);


    combined.fill(Qt::transparent);

    QPainter painter(&combined);

    // Draw scribbles into image
    for (const ScribbleInfo& scrib : std::as_const(saved_scribbles)) {
        painter.drawImage(scrib.qrect.topLeft(), scrib.original_image);
    }

    painter.end();

    // Save image
    combined_scribbles = combined.copy();

    QDir().mkpath("saved_scribbles");

    QString filename = "saved_scribbles/combined_scribbles.png";

    if (!combined.save(filename)) {
        std::cerr << "Failed to save scribbles." << std::endl;
    }
}

void ScribbleContext::saveScribblesWithoutImageSize()
{
    // When there is no image currently loaded, save with respect to the
    // combined size of all scribbles instead

    const QVector<ScribbleInfo>& scribbles = saved_scribbles;

    if (scribbles.empty()) {
        std::cerr << "No scribbles to save." << std::endl;
        return;
    }

    // Compute bounding box of all scribbles & create image
    QRect bounding;

    for (const ScribbleInfo& s : scribbles) {
        bounding = bounding.united(s.qrect);
    }

    QImage combined(bounding.size(), QImage::Format_ARGB32);

    combined.fill(Qt::transparent);

    // Draw scribbles into image
    QPainter painter(&combined);

    // Draw each scribble into the combined image
    for (const ScribbleInfo& s : scribbles) {
        // shift into local coordinates of the bounding box
        QPoint topLeft = s.qrect.topLeft() - bounding.topLeft();

        painter.drawImage(topLeft, s.original_image);
    }

    painter.end();

    // save image
    combined_scribbles = combined.copy();

    QDir().mkpath("saved_scribbles");

    QString filename = "saved_scribbles/combined_scribbles.png";

    if (!combined.save(filename)) {
        std::cerr << "Failed to save scribbles." << std::endl;
    }
}

QRect rect_type_to_QRect(rect_type const & rect)
{
    return QRect(rect.x(), rect.y(), rect.width(), rect.height());
}

QRect rectFromImage(const QImage& image)
{
    // create a rect that retains the original coordinates of the scribble within the image
    QRect rect;

    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            QColor p{image.pixelColor(x, y)};
            if (p.alpha() > 0) {
                rect = rect.isNull()
                    ? QRect(x, y, 1, 1)
                    : rect.united(QRect(x, y, 1, 1));
            }
        }
    }


    return rect;
}

ScribbleInfo ScribbleContext::createScribblesFromQImage(QImage image, label_type label){
    // for transformed scribbles - easily create a scribble in order to use the colorize function

    QRect rect = rectFromImage(image);
    return ScribbleInfo(image.copy(rect), rect, label);
}

// proxy colorize method
QImage ScribbleContext::colorize(const ScribbleInfo& scribble){

    textoons_colorization_context small_context(
        0,
        0,
        size.width(),
        size.height(),
        cell_size,
        points);

    small_context.append_scribble(scribble);
    QImage segmnt(size, QImage::Format_ARGB32);
    segmnt.fill(Qt::transparent);

    std::vector<std::pair<rect_type, label_type>> labeling =
        lazybrush::grid_of_quadtrees_colorizer::colorize(small_context, true);

    QPainter painter(&segmnt);

    //grid_type const & working_grid = small_context.working_grid();
    for (auto const & node : labeling)
    {
        if (node.second != textoons_colorization_context::label_undefined &&
            node.second != textoons_colorization_context::label_implicit_surrounding)
        {
            int const index = node.second;
            if (index >= 0 && index < 128)
            {
                QColor c =
                    QColor
                    (
                        the_palette[index][0],
                        the_palette[index][1],
                        the_palette[index][2]
                        );
                painter.fillRect(rect_type_to_QRect(node.first), c);
            }
        }
    }

    // QDir().mkpath("saved_scribbles");

    // QString filename = "saved_scribbles/segmentation.png";

    // if (!segmnt.save(filename)) {
    //     std::cerr << "Failed to save scribbles." << std::endl;
    // }

    return segmnt;
}
