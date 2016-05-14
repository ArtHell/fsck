#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    performRepair = true;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_startButton_clicked()
{
    disk_image = ui->ChooseLineEdit->text();
    FileSystem* ext2 = new FileSystem(disk_image.toStdString().c_str(), performRepair, ui->logTextBrowser, ui->checkProgressBar);
    delete ext2;
}



void MainWindow::on_checkBox_stateChanged(int arg1)
{
    performRepair = !performRepair;
}

void MainWindow::on_chooseToolButton_clicked()
{
    ui->checkProgressBar->setValue(0);
    ui->logTextBrowser->clear();
    ChooseDisk chooseDisk;
    ui->ChooseLineEdit->setText(chooseDisk.openFile());
}
