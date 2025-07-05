#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>

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

    void handleConversionError(const QString &errorMessage);

    void on_stopConversionBtn_clicked();

    void updateProgressBar(int current, int total, const QString &message);
    void on_selectInputDirectoryBtn_clicked();

    void on_selectOutputDirectoryBtn_clicked();

    void on_periodicalConversionBtn_clicked();

    void disableUi();
    void enableUi();

private:
    Ui::MainWindow *ui;

    QThread* m_converterThread;
    FileConverter* m_converter;

};
#endif // MAINWINDOW_H
