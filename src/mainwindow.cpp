#include "mainwindow.h"
#include <iostream>
#include <QHBoxLayout>
#include <QDir>
#include <QPainter>

MainWindow::MainWindow(QWidget *parent, ScribbleContext* ctx)
    : QMainWindow(parent)

{
    scribble_context = ctx;

    QWidget* central = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(central);

    save_button = new QPushButton("Save Scribbles", this);
    layout->addWidget(save_button);

    setCentralWidget(central);

    connect(save_button, &QPushButton::clicked,
            this, &MainWindow::onSaveScribblesClicked);

    // lazyb_window = new lzwindow(scribble_context);
    // lazyb_window->show();
}

void MainWindow::onSaveScribblesClicked(){
//     if (scribble_context->size().isEmpty()){
//         scribble_context->saveScribblesWithoutImageSize();
//     } else {
//         scribble_context->saveScribblesWithImageSize();
//     }
}
