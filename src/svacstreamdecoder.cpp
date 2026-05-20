#include "svacstreamdecoder.h"
#include <QDebug>
#include <QApplication>
#include "ESDecoder.h"
#include "PSDecoder.h"
#include "config.h"
#include "mainwindow.h"
#include <QCoreApplication>

SVACStreamDecoder::SVACStreamDecoder(MainWindow* window, QObject *parent)
    : QObject(nullptr)
    , mainWindow(window)
    , isRunning(false)
    , isPaused(false)
    , frameWidth(0)
    , frameHeight(0)
{
    // 将解密器移动到工作线程
    this->moveToThread(&workerThread);

    // 启动工作线程
    workerThread.start();
}

SVACStreamDecoder::~SVACStreamDecoder()
{
    // 停止解密
    onStopDecryption();

    // 停止并等待工作线程结束
    if (workerThread.isRunning()) {
        workerThread.quit();
        if (!workerThread.wait(3000)) { // 等待最多3秒
            workerThread.terminate(); // 如果线程没有正常退出，强制终止
            workerThread.wait(); // 等待线程完全终止
        }
    }

    // 清理资源
    cleanupDecoder();
}

void SVACStreamDecoder::onFileSelected(const QString& filePath)
{
    // 停止当前解密
    //onStopDecryption();

    currentFile = filePath;

    // TODO: 初始化解密器
    if (!initDecoder(filePath)) {
        qDebug() << "初始化解密器失败";
        return;
    }
}

void SVACStreamDecoder::onStartDecryption()
{
    qDebug() << "Current thread ID in onStartDecryption:" << QThread::currentThreadId();
    if (!isRunning) {
        isRunning = true;
        isPaused = false;    
        // 使用 QMetaObject::invokeMethod 确保在工作线程中执行
        QMetaObject::invokeMethod(this, &SVACStreamDecoder::decryptLoop, Qt::QueuedConnection);
    } else if (isPaused) {
        isPaused = false;
        decoder->setPause(false);
    }
}

void SVACStreamDecoder::onPauseDecryption()
{

    decoder->setPause(true);
    isPaused = true;
}

void SVACStreamDecoder::onStopDecryption()
{

    if (decoder) {
        decoder->setStop(true);
    }
    isRunning = false;
    isPaused = false;
}

void SVACStreamDecoder::decryptLoop()
{
    if (!decoder) {
        qDebug() << "Decoder is null, cannot start decryption";
        isRunning = false;
        return;
    }

    try {
        qDebug() << "Starting decryption loop in thread:" << QThread::currentThreadId();

        // 检查线程状态
        if (!QThread::currentThread() || !workerThread.isRunning()) {
            qDebug() << "Thread is not running properly";
            isRunning = false;
            return;
        }

        // parse 函数内部已经在循环解析帧，这里直接调用即可
        decoder->parse();

        qDebug() << "Decryption loop finished normally";
    } catch (const std::exception& e) {
        qDebug() << "Error in decryptLoop:" << e.what();
    } catch (...) {
        qDebug() << "Unknown error in decryptLoop";
    }

    // 当 parse 返回时，说明解析结束或出错
    isRunning = false;
    cleanupDecoder();

    // 发送信号通知主线程解密已完成
    emit decryptionFinished();
}


bool SVACStreamDecoder::initDecoder(const QString& filePath)
{
    qDebug() << "Initializing decoder for file:" << filePath;

    QString suffix = QFileInfo(filePath).suffix();

    std::string streamId = ConfigReader::Instance().getValue("Player", "stream_id");

    if (suffix == "es") {
        decoder = std::make_shared<ESDecoder>(filePath.toStdString(), streamId);
        decoder->getPlayRightInfo((unsigned int &)spanDays, (unsigned int &)spanCounts);
        decoder->scanIDRs();
    }
    else if (suffix == "ps" || suffix == "mp4") {
        decoder = std::make_shared<PSDecoder>(filePath.toStdString(), streamId);
        decoder->scanIDRs();
    }
    else {
        return false;
    }

    // 设置帧回调
    decoder->setOnFrame([this](const Frame& frame) {
        if (!mainWindow) return;

        // 设置视频尺寸
        frameWidth = frame.width;
        frameHeight = frame.height;
        mainWindow->setVideoSize(frameWidth, frameHeight);

        // qDebug() << "Current thread ID:" << QThread::currentThreadId()
        //          << "Main thread ID:" << QApplication::instance()->thread()->currentThreadId();
        // 确保在主线程中处理
        QMetaObject::invokeMethod(mainWindow, [this, frame]() {
            // 更新YUV数据
            mainWindow->receiveYUVFrame(
                frame.data.data(),                    // Y数据
                frame.data.data() + frame.size * 2/3, // U数据
                frame.data.data() + frame.size * 5/6, // V数据
                frame.size * 2/3,                     // Y大小
                frame.size / 6,                       // U大小
                frame.size / 6,                     // V大小
                spanDays,
                spanCounts
            );
            if (frame.keyFrame) {
                qDebug() << "frame Num: " <<frame.frameNum;
                int sliderValue = (double)(frame.frameNum+1 )* 100 / decoder->getKeyFrameSum();
                qDebug() << "sliderValue: " << sliderValue;
                mainWindow->moveSlider(sliderValue);
            }

        }, Qt::QueuedConnection);

        QThread::msleep(20);
    });



    return true;
}

void SVACStreamDecoder::cleanupDecoder()
{

    qDebug() << "Cleaning up decoder.";
    if (decoder) {
        decoder.reset();
    }
    // 清理缓冲区
    yBuffer.clear();
    uBuffer.clear();
    vBuffer.clear();
}


void SVACStreamDecoder::exportFile(const QString& filePath, QFile& outfile, QString deadline, QString playCounts) {

    QString suffix = QFileInfo(filePath).suffix();
    if (suffix == "mp4" || suffix == "ps") {
        Channel::Ptr downloadChannel;
        exporter = std::make_shared<PSDecoder>(filePath.toStdString());
        exporter->setOnOrignFrame([&](const Frame& frame) {

            std::string streamId = ConfigReader::Instance().getValue("Player", "stream_id");
            if (!downloadChannel) {
                downloadChannel = SDKManager::Instance().createChannel(streamId, ChannelType::DownloadChannel);
            }
            Frame downloadFrame;
            downloadChannel->doWork(const_cast<Frame&>(frame), downloadFrame);
            outfile.write((char*)downloadFrame.data.data(), downloadFrame.size);
            std::cout << "NAL Unit (Frame) size: " << frame.size << " bytes" << std::endl;
        });



        exporter->parse();
        //主动释放，内部调用下载完成接口
        downloadChannel.reset();

        outfile.flush();        // 确保写入数据已经落盘

        outfile.seek(0);        // 回到文件开头
        size_t fileSize = outfile.size();
        int bufSize =  3*1024*1024;

        if (fileSize < bufSize) {
            bufSize = fileSize;
        }

        QByteArray head = outfile.read(bufSize);
        int ret = SDKManager::Instance().setPlayRightInfo((unsigned char *)head.data(), bufSize,
                                            (char*)deadline.toStdString().c_str(), playCounts.toInt());
        if (ret == 0 ) {
            qDebug() << "SetPlayRightInfo Suc deadline: " << deadline<< " playCounts: " << playCounts;
        }

        qDebug() << "Exporter work done:" << outfile.fileName();
    }

}
