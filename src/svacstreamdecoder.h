#ifndef SVACSTREAMDECODER_H
#define SVACSTREAMDECODER_H

#include <QObject>
#include <QThread>
#include <QString>
#include <memory>
#include "defs.h"

class MainWindow;

class SVACStreamDecoder : public QObject
{
    Q_OBJECT

public:
    explicit SVACStreamDecoder(MainWindow* window, QObject *parent = nullptr);
    ~SVACStreamDecoder();

    void onFileSelected(const QString& filePath);
    void onStartDecryption();
    void onPauseDecryption();
    void onStopDecryption();

signals:
    void decryptionFinished();

private slots:
    void decryptLoop();

private:
    bool initDecoder(const QString& filePath);
    void cleanupDecoder();
    bool decryptFrame();

    MainWindow* mainWindow;
    QThread workerThread;
    bool isRunning;
    bool isPaused;
    QString currentFile;
    std::shared_ptr<class StreamDecoder> decoder;
    int frameWidth;
    int frameHeight;
    QByteArray yBuffer;
    QByteArray uBuffer;
    QByteArray vBuffer;
};

#endif // SVACSTREAMDECODER_H 