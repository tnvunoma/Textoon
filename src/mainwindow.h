#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
//#include <scribblecontext.h>
#include <lazybrush/lazybrush_components/window.h>

class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    ScribbleContext* scribble_context;
    lzwindow* lazyb_window;
    QPushButton* save_button;

public:
    explicit MainWindow(QWidget *parent = nullptr, ScribbleContext* ctx = nullptr);
    void onSaveScribblesClicked();
};

#endif // MAINWINDOW_H
