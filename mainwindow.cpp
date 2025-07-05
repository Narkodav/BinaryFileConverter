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
    ui->conversionProgress->hide();

    m_converterThread = new QThread();
    m_converter = new FileConverter();

    m_converter->moveToThread(m_converterThread);

    connect(m_converter, &FileConverter::finishedConversion, this, &MainWindow::finishedConversion);
    connect(m_converter, &FileConverter::errorOccurred, this, &MainWindow::handleConversionError);
    connect(m_converter, &FileConverter::progressChanged, this, &MainWindow::updateProgressBar);

    void progressChanged(int current, int total, const QString &message);
    void errorOccurred(const QString &errorMessage);
    void finishedConversion(bool success);
}

void MainWindow::finishedConversion(bool success)
{
    m_converterThread->quit();
    m_converterThread->wait();
    enableUi();
}

MainWindow::~MainWindow()
{
    m_converterThread->quit();
    m_converterThread->wait();
    delete m_converter;
    delete ui;
}

void MainWindow::on_oneTimeConversionBtn_clicked()
{
    ui->stopConversionBtn->show();
    ui->conversionProgress->show();
    ui->oneTimeConversionBtn->hide();
    ui->periodicalConversionBtn->hide();
    m_converterThread->start();
    emit m_converter->convertSingleTime(
        ui->selectedInputDirectory->text(),
        ui->selectedOutputDirectory->text(),
        ui->fileMaskInput->text(),
        ui->valueMaskInput->text(),
        ui->shouldDeleteSourceFiles->isChecked(),
        ui->shouldOverwriteConflicts->isChecked(),
        true
        );
}

void MainWindow::handleConversionError(const QString &errorMessage)
{
    QMessageBox::critical(
        this,
        tr("Conversion Error"),
        errorMessage,
        QMessageBox::Ok,
        QMessageBox::Ok
        );
}

void MainWindow::on_stopConversionBtn_clicked()
{
    m_converter->requestInterrupt();
    m_converterThread->quit();
    m_converterThread->wait();
}

void MainWindow::updateProgressBar(int current, int total, const QString &message)
{
    ui->conversionProgress->setMaximum(total);
    ui->conversionProgress->setValue(current);
    ui->conversionProgress->setFormat(message + " %p%");
}

void MainWindow::on_selectInputDirectoryBtn_clicked()
{
    ui->selectedInputDirectory->setText(QFileDialog::getExistingDirectory());
}

void MainWindow::on_selectOutputDirectoryBtn_clicked()
{
    ui->selectedOutputDirectory->setText(QFileDialog::getExistingDirectory());
}

void MainWindow::on_periodicalConversionBtn_clicked()
{
    disableUi();
    m_converterThread->start();
    emit m_converter->convertPeriodical(
        ui->selectedInputDirectory->text(),
        ui->selectedOutputDirectory->text(),
        ui->fileMaskInput->text(),
        ui->valueMaskInput->text(),
        ui->shouldDeleteSourceFiles->isChecked(),
        ui->shouldOverwriteConflicts->isChecked(),
        true,
        ui->timeInterval->value()
        );
}

void MainWindow::disableUi()
{
    ui->stopConversionBtn->show();
    ui->conversionProgress->show();
    ui->oneTimeConversionBtn->hide();
    ui->periodicalConversionBtn->hide();

    ui->fileMaskInput->setReadOnly(true);
    ui->valueMaskInput->setReadOnly(true);
    ui->selectedInputDirectory->setReadOnly(true);
    ui->selectedOutputDirectory->setReadOnly(true);
    ui->shouldDeleteSourceFiles->setEnabled(false);
    ui->shouldOverwriteConflicts->setEnabled(false);
    ui->timeInterval->setReadOnly(true);

    ui->selectInputDirectoryBtn->setEnabled(false);
    ui->selectOutputDirectoryBtn->setEnabled(false);
}

void MainWindow::enableUi()
{
    ui->stopConversionBtn->hide();
    ui->conversionProgress->hide();
    ui->oneTimeConversionBtn->show();
    ui->periodicalConversionBtn->show();

    ui->fileMaskInput->setReadOnly(false);
    ui->valueMaskInput->setReadOnly(false);
    ui->selectedInputDirectory->setReadOnly(false);
    ui->selectedOutputDirectory->setReadOnly(false);
    ui->shouldDeleteSourceFiles->setEnabled(true);
    ui->shouldOverwriteConflicts->setEnabled(true);
    ui->timeInterval->setReadOnly(false);

    ui->selectInputDirectoryBtn->setEnabled(true);
    ui->selectOutputDirectoryBtn->setEnabled(true);
}
