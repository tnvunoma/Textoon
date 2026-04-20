#include "scribblecontext.h"
#include <QImage>
#include <QPainter>
#include <iostream>
#include <QDir>

//ScribbleContext::ScribbleContext() {}


void ScribbleContext::storeScribbles(const QVector<scribble>& scribbles, const QSize m_size)
{
    // Stores given scribbles. Optional argument m_size indicates the size of an original image
    // to save with respect to
    saved_scribbles.clear();

    for (const scribble& s : scribbles)
    {
        saved_scribbles.push_back({s.image(), s.rect()});
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
        painter.drawImage(scrib.rect.topLeft(), scrib.image);
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
        bounding = bounding.united(s.rect);
    }

    QImage combined(bounding.size(), QImage::Format_ARGB32);

    combined.fill(Qt::transparent);

    // Draw scribbles into image
    QPainter painter(&combined);

    // Draw each scribble into the combined image
    for (const ScribbleInfo& s : scribbles) {
        // shift into local coordinates of the bounding box
        QPoint topLeft = s.rect.topLeft() - bounding.topLeft();

        painter.drawImage(topLeft, s.image);
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

