#ifndef CHOOSEDISK_H
#define CHOOSEDISK_H
#include <QFileDialog>

class ChooseDisk : public QWidget
{
public:
    QString openFile()
    {
        QString filename = QFileDialog::getOpenFileName(
                    this,
                    tr("Choose device"),
                    QDir::currentPath(),
                    tr("All files (*)") );
        return filename;
    }
};

#endif // CHOOSEDISK_H
