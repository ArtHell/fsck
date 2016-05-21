#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include "filesystem.h"
#include "choosedisk.h"
using namespace std;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_startButton_clicked(); // start checking

    void on_chooseToolButton_clicked(); // choose disk image

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
