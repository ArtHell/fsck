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
    void on_startButton_clicked();

    void on_checkBox_stateChanged(int arg1);

    void on_chooseToolButton_triggered(QAction *arg1);

    void on_chooseToolButton_clicked();

private:
    Ui::MainWindow *ui;
    bool performRepair;
    QString disk_image;

};

#endif // MAINWINDOW_H
