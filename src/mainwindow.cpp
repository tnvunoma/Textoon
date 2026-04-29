#include "mainwindow.h"
#include <iostream>
#include <QHBoxLayout>
#include <QDir>
#include <QPainter>

MainWindow::MainWindow(QWidget *parent, ScribbleContext* ctx)
    : QMainWindow(parent),
    scribble_context(ctx)
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(central);

    save_button = new QPushButton("Save Scribbles", this);
    layout->addWidget(save_button);

    setCentralWidget(central);

    connect(save_button, &QPushButton::clicked,
            this, &MainWindow::onSaveScribblesClicked);
}

void MainWindow::onSaveScribblesClicked()
{
    if (scribble_context->size.isEmpty())
    {
        scribble_context->saveScribblesWithoutImageSize();
    }
    else
    {
        scribble_context->saveScribblesWithImageSize();

        // optional debug: run LazyBrush segmentation on last scribble image
        if (!scribble_context->saved_scribbles.isEmpty())
        {
            const auto& last = scribble_context->saved_scribbles.back();

            QImage seg = scribble_context->imgColorize(last.original_image);

            QDir().mkpath("saved_scribbles");
            seg.save("saved_scribbles/debug_segmentation.png");
        }
    }
}
