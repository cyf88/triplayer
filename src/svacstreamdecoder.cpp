#include "svacstreamdecoder.h"
#include <QDebug>
#include "ESDecoder.h"
#include "PSDecoder.h"
#include "config.h"
#include "mainwindow.h"
#include <QCoreApplication>

SVACStreamDecoder::SVACStreamDecoder(MainWindow* window, QObject *parent)
    : QObject(parent)
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
    onStopDecryption();
    
    currentFile = filePath;
    
    // TODO: 初始化解密器
    if (!initDecoder(filePath)) {
        qDebug() << "初始化解密器失败";
        return;
    }
}

void SVACStreamDecoder::onStartDecryption()
{
    if (!isRunning) {
        isRunning = true;
        isPaused = false;
        // 使用 QMetaObject::invokeMethod 确保在工作线程中执行
        QMetaObject::invokeMethod(this, &SVACStreamDecoder::decryptLoop, Qt::QueuedConnection);
    } else if (isPaused) {
        isPaused = false;
    }
}

void SVACStreamDecoder::onPauseDecryption()
{
    isPaused = true;
}

void SVACStreamDecoder::onStopDecryption()
{
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
        qDebug() << "Starting decryption loop in thread:" << QThread::currentThread();
        
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

static std::string getSuffix(const std::string& str) {
    size_t pos = str.rfind('.'); // 找到最后一个 '.'
    if (pos != std::string::npos && pos + 1 < str.size()) {
        return str.substr(pos + 1); // 返回 '.' 之后的部分
    }
    return ""; // 如果没有 '.'，返回空字符串
}

bool SVACStreamDecoder::initDecoder(const QString& filePath)
{
    qDebug() << "Initializing decoder for file:" << filePath;

    std::string suf = getSuffix(filePath.toStdString());
    std::string streamId = ConfigReader::Instance().getValue("Player", "stream_id");

    if (suf == "es") {
		decoder = std::make_shared<ESDecoder>(filePath.toStdString(), streamId);
	}
	else if (suf == "ps") {
		decoder = std::make_shared<PSDecoder>(filePath.toStdString(), streamId);
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
        
        // 确保在主线程中处理
        QMetaObject::invokeMethod(mainWindow, [this, frame]() {
            // 更新YUV数据
            mainWindow->receiveYUVFrame(
                frame.data.data(),                    // Y数据
                frame.data.data() + frame.size * 2/3, // U数据
                frame.data.data() + frame.size * 5/6, // V数据
                frame.size * 2/3,                     // Y大小
                frame.size / 6,                       // U大小
                frame.size / 6                        // V大小
            );
        }, Qt::QueuedConnection);
    });

    return true;
}

void SVACStreamDecoder::cleanupDecoder()
{
    qDebug() << "Cleaning up decoder.";
    // 清理缓冲区
    yBuffer.clear();
    uBuffer.clear();
    vBuffer.clear();
} 
