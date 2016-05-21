#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_startButton_clicked()
{
    FileSystem* ext2 = new FileSystem(ui->ChooseLineEdit->text().toStdString().c_str(),
                                      !(ui->checkBox->isChecked()), ui->logTextBrowser,
                                      ui->checkProgressBar);
    delete ext2;
}

void MainWindow::on_chooseToolButton_clicked()
{
    ui->checkProgressBar->setValue(0);
    ui->logTextBrowser->clear();
    ChooseDisk chooseDisk;
    ui->ChooseLineEdit->setText(chooseDisk.openFile());
}
