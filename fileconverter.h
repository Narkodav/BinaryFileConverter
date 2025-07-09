#ifndef FILECONVERTER_H
#define FILECONVERTER_H

#include <QObject>
#include <QDir>
#include <QThread>
#include <QDataStream>
#include <QIODevice>
#include <QFile>
#include <QString>
#include <QFileDevice>
#include <QTimer>
#include <QEventLoop>
#include <QMetaObject>
#include <QAtomicInteger>

class FileConverter : public QObject
{
    Q_OBJECT

public:
    FileConverter() {
        connect(this, &FileConverter::requestInterrupt, this, &FileConverter::interruptRequested, Qt::DirectConnection);
        connect(this, &FileConverter::convertSingleTime, this, [this](const QString &inputDir,
                                                                      const QString &outputDir,
                                                                      const QString &fileMask,
                                                                      const QString &byteMask,
                                                                      bool deleteInputAfterConversion,
                                                                      bool overwriteExistingFiles,
                                                                      bool recursiveSearch) {
            this->resetInterruptFlag();
            QMetaObject::invokeMethod(this, "convert",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, inputDir),
                                      Q_ARG(QString, outputDir),
                                      Q_ARG(QString, fileMask),
                                      Q_ARG(QString, byteMask),
                                      Q_ARG(bool, deleteInputAfterConversion),
                                      Q_ARG(bool, overwriteExistingFiles),
                                      Q_ARG(bool, recursiveSearch));
        });

        connect(this, &FileConverter::convertPeriodical, this, [this](const QString &inputDir,
                                                                      const QString &outputDir,
                                                                      const QString &fileMask,
                                                                      const QString &byteMask,
                                                                      bool deleteInputAfterConversion,
                                                                      bool overwriteExistingFiles,
                                                                      bool recursiveSearch,
                                                                      int intervalSeconds) {
            this->resetInterruptFlag();
            QMetaObject::invokeMethod(this, "convertContinuously",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, inputDir),
                                      Q_ARG(QString, outputDir),
                                      Q_ARG(QString, fileMask),
                                      Q_ARG(QString, byteMask),
                                      Q_ARG(bool, deleteInputAfterConversion),
                                      Q_ARG(bool, overwriteExistingFiles),
                                      Q_ARG(bool, recursiveSearch),
                                      Q_ARG(int, intervalSeconds));
        });
    };

private:
    void convertFile(
        const QString &inputPath,
        const QString &outputDir,
        uint64_t mask,
        bool overwriteExisting
        ) {

        QFileInfo fileInfo(inputPath);
        QString outputPath = outputDir + "/" + fileInfo.fileName();

        if(QFile::exists(outputPath)) {
            if(!overwriteExisting) {
                outputPath = generateUniqueName(outputDir, fileInfo.fileName());
            } else {
                QFile::remove(outputPath);
            }
        }

        QFile inputFile(inputPath);
        QFile outputFile(outputPath);

        if (!inputFile.open(QIODevice::ReadOnly)) {
            throw std::runtime_error(
                QString("Cannot open input file: %1").arg(inputFile.errorString()).toStdString()
                );
        }

        if (!outputFile.open(QIODevice::WriteOnly)) {
            throw std::runtime_error(
                QString("Cannot create output file: %1").arg(outputFile.errorString()).toStdString()
                );
        }

        QDataStream input(&inputFile);
        QDataStream output(&outputFile);
        input.setByteOrder(QDataStream::LittleEndian);
        output.setByteOrder(QDataStream::LittleEndian);

        uint64_t value;
        uint64_t fileSize = input.device()->size();
        uint64_t chunkedPartSize = fileSize / 8;
        uint64_t remains = fileSize - chunkedPartSize * 8;

        for(size_t i = 0; i < chunkedPartSize; ++i) {
            input >> value;
            if (input.status() != QDataStream::Ok) {
                throw std::runtime_error("Input stream read error");
            }

            output << (value ^ mask);
            if (output.status() != QDataStream::Ok) {
                throw std::runtime_error("Output stream write error");
            }
        }

        if (remains > 0) {
            char buffer[8] = {0};
            uint64_t bytesRead = input.device()->read(buffer, remains);

            if (bytesRead == remains) {
                for (size_t i = 0; i < remains; ++i) {
                    buffer[i] ^= (mask >> (56 - i * 8)) & 0xFF; //little endian
                }
                output.writeRawData(buffer, remains);
            }
        }

        inputFile.close();
        outputFile.close();
    }

    QString generateUniqueName(const QString &dir, const QString &fileName) {
        QFileInfo fileInfo(fileName);
        QString baseName = fileInfo.baseName();
        QString extension = fileInfo.completeSuffix();

        QString newPath;
        int counter = 1;

        do {
            newPath = QString("%1/%2_(%3).%4")
                          .arg(dir)
                          .arg(baseName)
                          .arg(counter)
                          .arg(extension);
            counter++;
        } while(QFile::exists(newPath));

        return newPath;
    }

    uint64_t createMask(const QString &byteMask)
    {
        QStringList bytes = byteMask.split(' ', Qt::SkipEmptyParts);
        if(bytes.size() != 8)
            throw std::runtime_error("incorrect number of bytes in the byte mask");
        uint64_t mask = 0;

        foreach(const QString &byte, bytes)
        {
            bool ok;
            uint8_t byteVal = byte.toUInt(&ok, 16);
            if(!ok)
                throw std::runtime_error("error converting byte mask");

            mask <<= 8;
            mask |= byteVal;
        }

        return mask;
    }

    inline bool conversionLoop(
        const QString &inputDir,
        const QString &outputDir,
        const QString &byteMask,
        const QStringList& files,
        bool deleteInputAfterConversion,
        bool overwriteExistingFiles,
        uint64_t mask)
    {
        emit progressChanged(0, 0, "Starting conversion");
        const int totalFiles = files.size();
        int convertedFiles = 0;
        foreach(const QString &file, files) {
            if(m_interrupted.loadAcquire()) {
                return false;
            }

            emit progressChanged(
                convertedFiles,
                totalFiles,
                QString("Converting: %1").arg(file)
                );

            convertFile(
                inputDir + "/" + file,
                outputDir,
                mask,
                overwriteExistingFiles
                );

            if(deleteInputAfterConversion) {
                QFile::remove(inputDir + "/" + file);
            }

            convertedFiles++;
        }
        return true;
    }

public slots:
    void convert(const QString &inputDir,
                 const QString &outputDir,
                 const QString &fileMask,
                 const QString &byteMask,
                 bool deleteInputAfterConversion,
                 bool overwriteExistingFiles,
                 bool recursiveSearch) {

        if(inputDir.isEmpty() || outputDir.isEmpty()) {
            emit errorOccurred("Input and output directories must be specified");
            emit finishedConversion(false);
            return;
        }

        try {
            emit progressChanged(0, 0, "Starting conversion");
            uint64_t mask = createMask(byteMask);
            QDir inputDirectory(inputDir);
            QStringList filters = fileMask.split(' ', Qt::SkipEmptyParts);

            // Get file list based on file mask
            QStringList files = inputDirectory.entryList(
                filters,
                QDir::Files | (recursiveSearch ? QDir::NoDotAndDotDot | QDir::AllEntries : QDir::NoDotAndDotDot)
                );

            if(files.size() == 0)
                emit finishedConversion(true);

            if(!conversionLoop(inputDir,outputDir,byteMask,files,
                                deleteInputAfterConversion,overwriteExistingFiles,mask))
            {
                emit progressChanged(0, 0, "Conversion cancelled");
                emit finishedConversion(false);
            }

            emit progressChanged(0, 0, "Conversion finished");
            emit finishedConversion(true);
        }
        catch(const std::exception &e) {
            emit errorOccurred(QString("Conversion failed: %1") + e.what());
            emit finishedConversion(false);
        }
    }

    void convertContinuously(const QString &inputDir,
                             const QString &outputDir,
                             const QString &fileMask,
                             const QString &byteMask,
                             bool deleteInputAfterConversion,
                             bool overwriteExistingFiles,
                             bool recursiveSearch,
                             int intervalSeconds) {

        if(inputDir.isEmpty() || outputDir.isEmpty()) {
            emit errorOccurred("Input and output directories must be specified");
            emit finishedConversion(false);
            return;
        }

        try {
            QEventLoop waitLoop;
            QTimer waitTimer;
            waitTimer.setSingleShot(true);

            connect(this, &FileConverter::requestInterrupt, &waitLoop, &QEventLoop::quit, Qt::DirectConnection);
            connect(&waitTimer, &QTimer::timeout, &waitLoop, &QEventLoop::quit);

            uint64_t mask = createMask(byteMask);
            QDir inputDirectory(inputDir);
            QStringList filters = fileMask.split(' ', Qt::SkipEmptyParts);

            while(!m_interrupted.loadAcquire()) {
                QStringList files = inputDirectory.entryList(
                    filters,
                    QDir::Files | (recursiveSearch ? QDir::NoDotAndDotDot | QDir::AllEntries : QDir::NoDotAndDotDot)
                    );

                if(!files.isEmpty() &&
                    !conversionLoop(inputDir,outputDir,byteMask,files,
                        deleteInputAfterConversion,overwriteExistingFiles,mask))
                    break;

                emit progressChanged(0, 0,
                                     QStringLiteral("Waiting %1 seconds").arg(intervalSeconds));
                waitTimer.start(intervalSeconds * 1000);
                waitLoop.exec();
            }

            emit progressChanged(0, 0, "Conversion cancelled");
            emit finishedConversion(true);
        }
        catch(const std::exception &e) {
            emit errorOccurred(QString("Conversion failed: %1").arg(e.what()));
            emit finishedConversion(false);
        }
    }

    void interruptRequested() { m_interrupted = true; };
    void resetInterruptFlag() { m_interrupted = false; };

signals:
    void progressChanged(int current, int total, const QString &message);
    void errorOccurred(const QString &errorMessage);
    void finishedConversion(bool success);
    void requestInterrupt();

    void convertSingleTime(const QString &inputDir,
                           const QString &outputDir,
                           const QString &fileMask,
                           const QString &byteMask,
                           bool deleteInputAfterConversion,
                           bool overwriteExistingFiles,
                           bool recursiveSearch);

    void convertPeriodical(const QString &inputDir,
                           const QString &outputDir,
                           const QString &fileMask,
                           const QString &byteMask,
                           bool deleteInputAfterConversion,
                           bool overwriteExistingFiles,
                           bool recursiveSearch,
                           int intervalSeconds);

private:
    QAtomicInteger<bool> m_interrupted{false};
};

#endif // FILECONVERTER_H
