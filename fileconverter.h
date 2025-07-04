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

class FileConverter : public QObject
{
    Q_OBJECT

public:
    FileConverter();

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
        while (!input.atEnd()) {
            input >> value;
            if (input.status() != QDataStream::Ok) {
                if (input.atEnd()) break; //stop at incomplete final chunk
                throw std::runtime_error("Input stream read error");
            }

            output << (value ^ mask);
            if (output.status() != QDataStream::Ok) {
                throw std::runtime_error("Output stream write error");
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

public slots:
    void convert(
        const QString &inputDir,          // Input directory path
        const QString &outputDir,         // Output directory path
        const QString &fileMask,          // File filter (e.g., "*.txt" or "testFile.bin")
        const QString &byteMask,          // 8-byte hex mask (e.g., "AF BF FF AF F1 F2 1F FF")
        bool deleteInputAfterConversion,  // Whether to delete source files
        bool overwriteExistingFiles,      // Whether to overwrite or auto-rename
        bool recursiveSearch              // Whether to search subdirectories
        ) {

        // Validate parameters
        if(inputDir.isEmpty() || outputDir.isEmpty()) {
            emit errorOccurred("Input and output directories must be specified");
            emit finishedConversion(false);
            return;
        }

        // Process files
        try {
            uint64_t mask = createMask(byteMask);
            QDir inputDirectory(inputDir);
            QStringList filters = fileMask.split(' ', Qt::SkipEmptyParts);

            // Get file list based on file mask
            QStringList files = inputDirectory.entryList(
                filters,
                QDir::Files | (recursiveSearch ? QDir::NoDotAndDotDot | QDir::AllEntries : QDir::NoDotAndDotDot)
                );

            const int totalFiles = files.size();
            int convertedFiles = 0;

            foreach(const QString &file, files) {
                if(QThread::currentThread()->isInterruptionRequested()) {
                    emit progressChanged(0, 0, "Conversion cancelled");
                    emit finishedConversion(false);
                    return;
                }

                emit progressChanged(
                    convertedFiles,
                    totalFiles,
                    QString("Converting: ").arg(file)
                    );

                convertFile(
                    inputDir + "/" + file,
                    outputDir,
                    mask,
                    overwriteExistingFiles
                    );

                // Delete source file if requested
                if(deleteInputAfterConversion) {
                    QFile::remove(inputDir + "/" + file);
                }

                // Update progress
                convertedFiles++;
            }

            emit finishedConversion(true);
        }
        catch(const std::exception &e) {
            emit errorOccurred(QString("Conversion failed: %1").arg(e.what()));
            emit finishedConversion(false);
        }
    }

signals:
    void progressChanged(int current, int total, const QString &message);
    void errorOccurred(const QString &errorMessage);
    void finishedConversion(bool success);
};

#endif // FILECONVERTER_H
