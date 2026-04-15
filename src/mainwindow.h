#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <scribblecontext.h>
#include <lazybrush/lazybrush_components/window.h>

class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    ScribbleContext scribble_context;
    lzwindow* lazyb_window;
    QPushButton* save_button_;

    void save_scribble_image_(const scribble& s, int color_index);

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void onSaveScribblesClicked();
};

#endif // MAINWINDOW_H
