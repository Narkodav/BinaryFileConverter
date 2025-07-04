#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "FileConverter.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_oneTimeConversionBtn_clicked();
    void finishedConversion(bool success);

private:
    Ui::MainWindow *ui;

    QThread* m_converterThread;
    FileConverter* m_converter;

};
#endif // MAINWINDOW_H
