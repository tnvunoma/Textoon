#pragma once

#include <QVector>
#include <QImage>
#include <lazybrush/lazybrush_components/scribble.hpp>
#include <QObject>

struct ScribbleInfo
{
    QImage image;
    QRect rect;
};

class ScribbleContext : public QObject
{
    Q_OBJECT
private:

public:
    QVector<ScribbleInfo> saved_scribbles;
    QSize size;
    QImage combined_scribbles;

    explicit ScribbleContext(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    //~ScribbleContext();

    void storeScribbles(const QVector<scribble>& scribbles, const QSize m_size = QSize());
    void saveScribblesWithImageSize();
    void saveScribblesWithoutImageSize();
};

//extern ScribbleContext scribble_context;

