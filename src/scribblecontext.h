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

    explicit ScribbleContext(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    void storeScribbles(const QVector<scribble>& scribbles)
    {
        saved_scribbles.clear();

        for (const scribble& s : scribbles)
        {
            saved_scribbles.push_back({s.image(), s.rect()});
        }
        emit scribbleAdded();
    }
signals:
    void scribbleAdded();
};
