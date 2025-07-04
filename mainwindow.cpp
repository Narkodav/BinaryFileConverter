#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->valueMaskInput->setText("");
    ui->valueMaskInput->setInputMask("");
    ui->valueMaskInput->setInputMask("HH HH HH HH HH HH HH HH");
    ui->stopConversionBtn->hide();

    m_converterThread = new QThread();
    m_converter = new FileConverter();

    m_converter->moveToThread(m_converterThread);

    connect(m_converterThread, &QThread::started, m_converter, &FileConverter::convert);
    connect(m_converter, &FileConverter::finishedConversion, this, &MainWindow::finishedConversion);
    connect(m_converter, &FileConverter::finishedConversion, m_converterThread, &QThread::quit);

    void progressChanged(int current, int total, const QString &message);
    void errorOccurred(const QString &errorMessage);
    void finishedConversion(bool success);
}

void MainWindow::finishedConversion(bool success)
{
    m_converterThread->exit();
    ui->stopConversionBtn->hide();
    ui->oneTimeConversionBtn->show();
    ui->periodicalConversionBtn->show();
}

MainWindow::~MainWindow()
{
    m_converterThread->deleteLater();
    m_converter->deleteLater();
    delete ui;
}

void MainWindow::on_oneTimeConversionBtn_clicked()
{
    m_converterThread->start();

    ui->stopConversionBtn->show();
    ui->oneTimeConversionBtn->hide();
    ui->periodicalConversionBtn->hide();
}



