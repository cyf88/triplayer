#ifndef SVACSTREAMDECODER_H
#define SVACSTREAMDECODER_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QByteArray>
#include <QMetaObject>
#include <QDebug>
#include <QMutex>
#include <QFile>
#include "StreamDecoder.h"

// 前向声明
class MainWindow;

class SVACStreamDecoder : public QObject
{
    Q_OBJECT

public:
    explicit SVACStreamDecoder(MainWindow* window, QObject *parent = nullptr);
    ~SVACStreamDecoder();

    void exportFile(const QString& filePath, QFile& outfile,  QString deadline, QString playCounts);

signals:
    void decryptionFinished();

public slots:
    // 接收来自MainWindow的控制信号
    void onFileSelected(const QString& filePath);
    void onStartDecryption();
    void onPauseDecryption();
    void onStopDecryption();

private slots:
    // 解密线程函数
    void decryptLoop();

private:
    // 初始化解密器
    bool initDecoder(const QString& filePath);
    // 清理解密器资源
    void cleanupDecoder();

private:
    MainWindow* mainWindow;
    QString currentFile;
    
    std::atomic<bool> isRunning;
    bool isPaused;
    
    // 解密线程
    QThread workerThread;

    // 视频参数
    int frameWidth;
    int frameHeight;
    
    // YUV缓冲区
    QByteArray yBuffer;
    QByteArray uBuffer;
    QByteArray vBuffer;

    int spanDays = 0;
    int spanCounts = 0;

    std::shared_ptr<StreamDecoder> decoder;

    std::shared_ptr<StreamDecoder> exporter;
    
    // TODO: 添加SVAC解密所需的其他成员变量
    // 例如：解密密钥、解密上下文等
};

#endif // SVACSTREAMDECODER_H 
