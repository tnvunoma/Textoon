#include "scribblecontext.h"
#include <QImage>
#include <QPainter>
#include <iostream>
#include <QDir>
#include <QString>
#include "lazybrush/lazybrush_components/window.h"



void ScribbleContext::storeSampledPoints(const std::vector<lz_colorization_context::input_point>& sampled_points){

    std::transform(sampled_points.begin(),
                   sampled_points.end(),
                   std::back_inserter(points),
                   [=](const lz_colorization_context::input_point& ip)
                   {
                       return textoons_colorization_context::input_point{ip.position, ip.intensity};
                    }
                   );
}

const QSize ScribbleContext::size(){
    return _size;
}

const QImage ScribbleContext::getScribblesAsImage(){
    return combined_scribbles;
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

    _size = m_size;
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
    QImage combined(_size, QImage::Format_ARGB32);


    combined.fill(Qt::transparent);

    QPainter painter(&combined);

    // Draw scribbles into image
    for (const ScribbleInfo& scrib : std::as_const(saved_scribbles)) {
        painter.drawImage(scrib.bounding.topLeft(), scrib.scrib_image);
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
        bounding = bounding.united(s.bounding);
    }

    QImage combined(bounding.size(), QImage::Format_ARGB32);

    combined.fill(Qt::transparent);

    // Draw scribbles into image
    QPainter painter(&combined);

    // Draw each scribble into the combined image
    for (const ScribbleInfo& s : scribbles) {
        // shift into local coordinates of the bounding box
        QPoint topLeft = s.bounding.topLeft() - bounding.topLeft();

        painter.drawImage(topLeft, s.scrib_image);
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

/*
segments also need to contain depth information - choose some arbitrary depth step
*/
// proxy colorize method

QImage ScribbleContext::createMaskByColor(const QRect& bounding, const QImage& original, const QColor& color){

    QImage mask(bounding.size(), QImage::Format_ARGB32);
    mask.fill(Qt::transparent);

    int b_height = bounding.height();
    int b_width = bounding.width();
    int tlx = bounding.topLeft().x();
    int tly = bounding.topLeft().y();

    QRgb* data{reinterpret_cast<QRgb*>(mask.bits())};
    for (int x = tlx; x < tlx+b_width; x++){
        for (int y = tly; y < tly+b_height; y++){
            int nw_index = (y - tly) * b_width + (x - tlx);

            QColor image_color(original.pixel(x, y));
            if (color == image_color){
                data[nw_index] = original.pixel(x, y);
            }
        }
    }

    return mask;
}

std::unordered_set<short> ScribbleContext::collectLabelsFromScribbles(){
    std::unordered_set<label_type> labels;
    for (const ScribbleInfo& scrib : std::as_const(saved_scribbles)){
        labels.insert(scrib.label());
    }
    return labels;
}

QVector<ScribbleInfo> ScribbleContext::extractScribblesFromQImage(const QImage& scribbles_image){
    //QImage scribbles_image(QString::fromStdString(scribble_filepath));
    QVector<ScribbleInfo> vec;

    std::map<short, QRect> rect_map;


    if (!scribbles_image.isNull()){

        // Construct QRects (bounding boxes) based on labels (image color)

        for (short label : collectLabelsFromScribbles()){
            int height = scribbles_image.size().height();
            int width = scribbles_image.size().width();

            const QRgb* data{reinterpret_cast<QRgb*>(const_cast<uchar*>(scribbles_image.bits()))};


            QColor target_color(
                static_cast<unsigned char>(the_palette[label][0]),
                static_cast<unsigned char>(the_palette[label][1]),
                static_cast<unsigned char>(the_palette[label][2]));

            for (int x = 0; x < width; x++){
                for (int y = 0; y < height; y++){
                    int index = y * width + x;

                    QColor image_color(data[index]);

                    if (image_color.alpha() > 0){
                        if (image_color == target_color){
                            if (rect_map.contains(label)){
                                rect_map[label] = rect_map[label].united(QRect(x, y, 1, 1));
                            } else {
                                rect_map[label] = QRect(x, y, 1, 1);
                            }
                        }
                    }
                }
            }
        }

        // Once QRects are obtained, the exact scribbles can be constructed, and these can be
        // added into the vector of scribbles and returned
        for (auto& [label, rect] : rect_map){
            QColor target_color(
                static_cast<unsigned char>(the_palette[label][0]),
                static_cast<unsigned char>(the_palette[label][1]),
                static_cast<unsigned char>(the_palette[label][2]));

            vec.push_back({createMaskByColor(rect, scribbles_image, target_color), rect, label});
        }
    } else {
        std::cerr << "Scribble extraction error: Unable to open image file" << std::endl;
    }
    return vec;
}



QImage ScribbleContext::colorize(const QImage& scribbles_image){
    return colorize(extractScribblesFromQImage(scribbles_image));
}

QImage ScribbleContext::colorize(const QVector<ScribbleInfo>& scribbles){

    textoons_colorization_context small_context(
        0,
        0,
        _size.width(),
        _size.height(),
        cell_size,
        points);

    /// TODO: Change colorize to work on a set of scribbles

    for (const ScribbleInfo& scribble : scribbles){
        small_context.append_scribble(scribble);
    }

    QImage segmnt(_size, QImage::Format_ARGB32);
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

    return segmnt;
}
