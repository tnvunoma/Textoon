#include "mainwindow.h"
#include <QHBoxLayout>
#include <QDir>
#include "lazybrush/lazybrush_components/window.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), lazyb_window(new lzwindow())

{
    connect(&scribble_context, &ScribbleContext::scribbleAdded,
            this, QOverload<>::of(&MainWindow::update));

    QWidget* central = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(central);

    // Create button
    save_button_ = new QPushButton("Save Scribbles", this);
    layout->addWidget(save_button_);

    setCentralWidget(central);

    // Connect button → slot
    connect(save_button_, &QPushButton::clicked,
            this, &MainWindow::onSaveScribblesClicked);

    lazyb_window->show();

    // choose some frame from an animation;
    // allow the user to scribble upon it;
    // save the scribbles to a file
}

void MainWindow::onSaveScribblesClicked()
{
    int index = 0;

    for (const ScribbleInfo& s : scribble_context.saved_scribbles)
    {
        QDir().mkpath("saved_scribbles");

        QString filename = QString("saved_scribbles/scribble_%1_x_%2_y_%3.png")
                               .arg(index++, 4, 10, QChar('0'))
                               .arg(s.rect.x())
                               .arg(s.rect.y());

        s.image.save(filename);
    }
}
