#include "mainwindow.h"
#include <QMessageBox>
#include "svacstreamdecoder.h"
#include <QDebug>
#include <QSignalBlocker>
#include "exportdialog.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isPlaying(false)
{
    // 设置窗口标题和大小
    setWindowTitle("35114密流播放器");
    resize(800, 600);

    // 创建中央部件和布局
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    mainLayout = new QVBoxLayout(centralWidget);

    // 创建视频播放器组件
    yuvPlayer = new YUVPlayer(this);

    progressSlider = new QSlider(Qt::Horizontal, this);
    progressSlider->setRange(0, kSliderMax);
    progressSlider->setValue(0);
    progressSlider->setEnabled(false);
    progressSlider->setTracking(true);

    progressTimer = new QTimer(this);
    progressTimer->setInterval(16);
    connect(progressTimer, &QTimer::timeout, this, &MainWindow::updateSmoothProgress);

    // 创建按钮布局
    buttonLayout = new QHBoxLayout();

    // 创建控制按钮
    playButton = new QPushButton("打开文件", this);
    pauseButton = new QPushButton("暂停", this);
    stopButton = new QPushButton("停止", this);
    exportButton = new QPushButton("导出", this);


    // 添加按钮到水平布局
    buttonLayout->addWidget(playButton);
    buttonLayout->addWidget(pauseButton);
    buttonLayout->addWidget(stopButton);
    buttonLayout->addWidget(exportButton);

    // 添加组件到主布局
    mainLayout->addWidget(yuvPlayer);
    mainLayout->addWidget(progressSlider);
    mainLayout->addLayout(buttonLayout);

    // 创建解密器
    decoder = new SVACStreamDecoder(this, this);

    // 连接信号和槽
    connect(playButton, &QPushButton::clicked, this, &MainWindow::onPlayButtonClicked);
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExportButtonClicked);

    // 连接解密器相关的信号和槽
    // connect(this, &MainWindow::fileSelected, decoder, &SVACStreamDecoder::onFileSelected);
    // connect(this, &MainWindow::startDecryption, decoder, &SVACStreamDecoder::onStartDecryption);
    // connect(this, &MainWindow::pauseDecryption, decoder, &SVACStreamDecoder::onPauseDecryption);
    // connect(this, &MainWindow::stopDecryption, decoder, &SVACStreamDecoder::onStopDecryption);

    connect(this, &MainWindow::fileSelected, decoder, &SVACStreamDecoder::onFileSelected, Qt::QueuedConnection);
    connect(this, &MainWindow::startDecryption, decoder, &SVACStreamDecoder::onStartDecryption, Qt::DirectConnection);
    connect(this, &MainWindow::pauseDecryption, decoder, &SVACStreamDecoder::onPauseDecryption, Qt::QueuedConnection);
    connect(this, &MainWindow::stopDecryption, decoder, &SVACStreamDecoder::onStopDecryption, Qt::DirectConnection);
    connect(this, &MainWindow::seekRequested, decoder, &SVACStreamDecoder::onSeek, Qt::DirectConnection);
    
    //接受解密结束的通知
    connect(decoder, &SVACStreamDecoder::decryptionFinished, this, &MainWindow::setDecryptionFinished);

    connect(progressSlider, &QSlider::sliderPressed, this, &MainWindow::onSliderPressed);
    connect(progressSlider, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);
    progressSlider->installEventFilter(this);

    // 初始化按钮状态
    pauseButton->setEnabled(false);
    stopButton->setEnabled(false);
}

MainWindow::~MainWindow()
{
}

void MainWindow::receiveYUVFrame(const uchar* yData, const uchar* uData, const uchar* vData,
                                int ySize, int uSize, int vSize, int days, int counts)
{
    // 检查数据有效性
    if (!yData || !uData || !vData || ySize <= 0 || uSize <= 0 || vSize <= 0) {
        qDebug() << "Invalid YUV data received:" << yData << uData << vData << ySize << uSize << vSize;
        return;
    }
    // 创建保存YUV数据的文件
    /*
    static int frameCount = 0;
    QString basePath = "E:/yuv_frames/";
    QDir dir(basePath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 保存Y分量
    QString yFileName = QString("%1frame_%2_y.yuv").arg(basePath).arg(frameCount);
    QFile yFile(yFileName);
    if (yFile.open(QIODevice::WriteOnly)) {
        yFile.write(reinterpret_cast<const char*>(yData), ySize);
        yFile.close();
    }

    // 保存U分量
    QString uFileName = QString("%1frame_%2_u.yuv").arg(basePath).arg(frameCount);
    QFile uFile(uFileName);
    if (uFile.open(QIODevice::WriteOnly)) {
        uFile.write(reinterpret_cast<const char*>(uData), uSize);
        uFile.close();
    }

    // 保存V分量
    QString vFileName = QString("%1frame_%2_v.yuv").arg(basePath).arg(frameCount);
    QFile vFile(vFileName);
    if (vFile.open(QIODevice::WriteOnly)) {
        vFile.write(reinterpret_cast<const char*>(vData), vSize);
        vFile.close();
    }

    frameCount++;
*/
  //  qDebug() << "MainWindow::receiveYUVFrame called with sizes:" << ySize << uSize << vSize;
    yuvPlayer->updateYUVFrame(yData, uData, vData, ySize, uSize, vSize);
    //OSD测试
    QString suffix = QFileInfo(currentFile).suffix();
    //只有导出文件才显示权限信息
    if (suffix == "es") {
        yuvPlayer->setDisplayText(NULL,  QString("剩余天数:%1 剩余次数:%2").arg(days).arg(counts));
    } else {
        yuvPlayer->setDisplayText(NULL, NULL);
    }

}

void MainWindow::setDecryptionFinished()
{
    progressTimer->stop();
    setProgressImmediate(100);
    progressSlider->setEnabled(false);
    playButton->setText("播放结束,重新打开");
    playButton->setEnabled(true);
    pauseButton->setEnabled(false);
    stopButton->setEnabled(false);
    isPlaying = false;
    currentFile.clear();
}

void MainWindow::setVideoSize(int width, int height)
{
    yuvPlayer->setFrameSize(width, height);
}

void MainWindow::onPlayButtonClicked()
{
    if (!isPlaying) {
        //没有暂停标志，用currentFile.isEmpty区分暂停和停止
        if (currentFile.isEmpty()) {
            // 如果没有打开文件，先打开文件
            QString fileName = QFileDialog::getOpenFileName(this,
                "打开SVAC文件", "", "SVAC文件 (*.ps *.mp4 *.es);;所有文件 (*.*)");

            if (fileName.isEmpty()) {
                return;  // 用户取消选择
            }

            currentFile = fileName;
            resetProgressSmooth();
            emit fileSelected(fileName);
        }

        // 开始播放
        isPlaying = true;
        beginPlaybackClock(sliderToPercent(progressSlider->value()));
        emit startDecryption();
        
        playButton->setText("播放中...");
        pauseButton->setEnabled(true);
        stopButton->setEnabled(true);
        progressSlider->setEnabled(true);
    }
}

void MainWindow::onPause()
{
    if (isPlaying) {
        isPlaying = false;
        pausePlaybackClock();
        progressTimer->stop();
        emit pauseDecryption();
           
        playButton->setText("继续播放");
        playButton->setEnabled(true);
        pauseButton->setEnabled(false);
        stopButton->setEnabled(true);
    }
}

void MainWindow::onStop()
{
    isPlaying = false;
    progressTimer->stop();
    emit stopDecryption();
    currentFile.clear();

    resetProgressSmooth();
    progressSlider->setEnabled(false);
    playButton->setText("打开文件");
    playButton->setEnabled(true);
    pauseButton->setEnabled(false);
    stopButton->setEnabled(false);
}

void MainWindow::onExportButtonClicked()
{
    ExportDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QDateTime endTime = dlg.getEndTime();
        QString playCounts = dlg.getPlayCounts();

        QString fileName = QFileDialog::getSaveFileName(this, "保存文件", "", "密流文件 (*.es)");
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::ReadWrite)) {
                decoder->exportFile(currentFile, file, endTime.toString("yyyy-MM-dd"), playCounts);
                file.close();
                QMessageBox::information(this, "导出成功", "密流文件已成功导出！");
            } else {
                QMessageBox::warning(this, "导出失败", "无法写入文件！");
            }
        }


    }
}

void MainWindow::moveSlider(int value) {
    setProgressImmediate(value);
}

int MainWindow::percentToSlider(int percent)
{
    percent = qBound(0, percent, 100);
    return percent * kSliderMax / 100;
}

int MainWindow::sliderToPercent(int sliderValue)
{
    sliderValue = qBound(0, sliderValue, kSliderMax);
    return sliderValue * 100 / kSliderMax;
}

int MainWindow::msToSlider(qint64 ms, qint64 durationMs)
{
    if (durationMs <= 0) {
        return 0;
    }
    ms = qBound<qint64>(0, ms, durationMs);
    return static_cast<int>(ms * kSliderMax / durationMs);
}

qint64 MainWindow::sliderToMs(int sliderValue, qint64 durationMs)
{
    if (durationMs <= 0) {
        return 0;
    }
    sliderValue = qBound(0, sliderValue, kSliderMax);
    return static_cast<qint64>(sliderValue) * durationMs / kSliderMax;
}

bool MainWindow::isSliderInteracting() const
{
    return sliderDragging || progressSlider->isSliderDown();
}

bool MainWindow::isClickOnSliderHandle(const QPoint& pos) const
{
    QStyleOptionSlider opt;
    opt.initFrom(progressSlider);
    opt.orientation = progressSlider->orientation();
    opt.minimum = progressSlider->minimum();
    opt.maximum = progressSlider->maximum();
    opt.sliderPosition = progressSlider->sliderPosition();
    opt.sliderValue = progressSlider->value();
    opt.singleStep = progressSlider->singleStep();
    opt.pageStep = progressSlider->pageStep();
    opt.upsideDown = progressSlider->invertedAppearance();

    const QRect handleRect = progressSlider->style()->subControlRect(
        QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, progressSlider);
    return handleRect.contains(pos);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == progressSlider && progressSlider->isEnabled()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            pressOnHandle = isClickOnSliderHandle(mouseEvent->pos());
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (sliderDragging) {
                QTimer::singleShot(0, this, [this]() {
                    if (sliderDragging) {
                        commitSliderSeek();
                    }
                });
            }
            pressOnHandle = false;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::beginSliderInteraction()
{
    if (sliderDragging) {
        return;
    }

    sliderDragging = true;
    seekAwaitingSync = false;
    progressTimer->stop();

    const int percent = sliderToPercent(progressSlider->value());
    decodeProgress = percent;
    if (totalDurationMs > 0) {
        decodeProgressMs = static_cast<qint64>(percent) * totalDurationMs / 100;
        playbackBaseMs = decodeProgressMs;
    }
    playbackTimer.restart();
}

void MainWindow::setMediaInfo(qint64 durationMs, qint64 fileSizeBytes)
{
    baseDurationMs = durationMs;
    totalDurationMs = durationMs;
    if (totalDurationMs <= 0 && fileSizeBytes > 0) {
        // 无 PTS 时长时按约 2Mbps 估算，保证进度条能匀速走
        totalDurationMs = fileSizeBytes * 8 * 1000 / (2 * 1000 * 1000);
        if (totalDurationMs < 3000) {
            totalDurationMs = 3000;
        }
    }
    baseDurationMs = totalDurationMs;
}

void MainWindow::syncPlaybackProgress(qint64 progressMs)
{
    if (isSliderInteracting()) {
        return;
    }

    if (totalDurationMs > 0) {
        progressMs = qBound<qint64>(0, progressMs, totalDurationMs);
    } else if (progressMs < 0) {
        progressMs = 0;
    }

    if (seekAwaitingSync) {
        if (totalDurationMs > 0) {
            const qint64 toleranceMs = qMax(totalDurationMs / 33, static_cast<qint64>(1000));
            const bool seekForward = seekTargetMs >= seekStartMs;
            const bool nearTarget = qAbs(progressMs - seekTargetMs) <= toleranceMs;
            const bool passedTarget = seekForward
                ? progressMs >= seekTargetMs
                : progressMs <= seekTargetMs;
            const bool leftStart = qAbs(progressMs - seekStartMs) > toleranceMs / 2;

            if (nearTarget || passedTarget || leftStart) {
                decodeProgressMs = progressMs;
                decodeProgress = static_cast<int>(progressMs * 100 / totalDurationMs);
                seekAwaitingSync = false;
            }
        } else {
            seekAwaitingSync = false;
            decodeProgress = static_cast<int>(progressMs);
            decodeProgressMs = progressMs;
        }
        return;
    }

    decodeProgressMs = progressMs;
    if (totalDurationMs > 0) {
        decodeProgress = static_cast<int>(progressMs * 100 / totalDurationMs);
    } else {
        decodeProgress = 0;
    }

    if (totalDurationMs <= 0) {
        if (!firstFrameReceived) {
            firstFrameReceived = true;
            if (!progressTimer->isActive()) {
                progressTimer->start();
            }
        }
        QSignalBlocker blocker(progressSlider);
        progressSlider->setValue(percentToSlider(decodeProgress));
        return;
    }

    if (!firstFrameReceived) {
        firstFrameReceived = true;
        playbackBaseMs = progressMs;
        playbackTimer.restart();
        if (!progressTimer->isActive()) {
            progressTimer->start();
        }
        QSignalBlocker blocker(progressSlider);
        progressSlider->setValue(msToSlider(progressMs, totalDurationMs));
        return;
    }

    const qint64 currentMs = playbackBaseMs + playbackTimer.elapsed();
    playbackBaseMs = qMax(currentMs, progressMs);
    playbackTimer.restart();
}

void MainWindow::setProgressImmediate(int percent)
{
    percent = qBound(0, percent, 100);
    decodeProgress = percent;

    if (totalDurationMs > 0) {
        decodeProgressMs = static_cast<qint64>(percent) * totalDurationMs / 100;
        playbackBaseMs = decodeProgressMs;
        playbackTimer.restart();
    } else {
        decodeProgressMs = 0;
    }

    if (isSliderInteracting()) {
        return;
    }

    QSignalBlocker blocker(progressSlider);
    progressSlider->setValue(percentToSlider(percent));
}

void MainWindow::resetProgressSmooth()
{
    baseDurationMs = 0;
    totalDurationMs = 0;
    playbackBaseMs = 0;
    decodeProgress = 0;
    decodeProgressMs = 0;
    seekTargetPercent = 0;
    seekTargetSlider = 0;
    seekStartMs = 0;
    seekTargetMs = 0;
    seekAwaitingSync = false;
    firstFrameReceived = false;
    QSignalBlocker blocker(progressSlider);
    progressSlider->setValue(0);
}

void MainWindow::beginPlaybackClock(int startPercent)
{
    startPercent = qBound(0, startPercent, 100);
    decodeProgress = startPercent;

    if (totalDurationMs > 0) {
        decodeProgressMs = static_cast<qint64>(startPercent) * totalDurationMs / 100;
        playbackBaseMs = decodeProgressMs;
    } else {
        decodeProgressMs = 0;
        playbackBaseMs = 0;
    }

    playbackTimer.restart();
    // 首次播放等第一帧解码后才开始移动；暂停恢复时直接继续
    if (firstFrameReceived && !progressTimer->isActive()) {
        progressTimer->start();
    }
}

void MainWindow::pausePlaybackClock()
{
    if (totalDurationMs > 0) {
        playbackBaseMs += playbackTimer.elapsed();
    }
}

void MainWindow::updateSmoothProgress()
{
    if (isSliderInteracting() || !isPlaying || !firstFrameReceived) {
        return;
    }

    int sliderValue = progressSlider->value();

    if (totalDurationMs > 0) {
        qint64 smoothMs = playbackBaseMs + playbackTimer.elapsed();

        // 未播完前保留结尾余量，避免滑块先到终点
        constexpr qint64 kEndGuardMs = 500;
        if (decodeProgressMs + kEndGuardMs < totalDurationMs) {
            smoothMs = qMin(smoothMs, totalDurationMs - kEndGuardMs);
        }

        sliderValue = msToSlider(smoothMs, totalDurationMs);
    } else if (sliderValue < percentToSlider(decodeProgress)) {
        sliderValue = qMin(sliderValue + 2, percentToSlider(decodeProgress));
    }

    if (progressSlider->value() != sliderValue) {
        QSignalBlocker blocker(progressSlider);
        progressSlider->setValue(sliderValue);
    }
}

void MainWindow::onSliderPressed()
{
    if (pressOnHandle) {
        beginSliderInteraction();
    }
}

void MainWindow::commitSliderSeek()
{
    sliderDragging = false;
    if (baseDurationMs > 0) {
        totalDurationMs = baseDurationMs;
    }

    seekStartMs = decodeProgressMs;
    seekTargetSlider = progressSlider->value();
    seekTargetPercent = sliderToPercent(seekTargetSlider);
    seekTargetMs = sliderToMs(seekTargetSlider, totalDurationMs);
    seekAwaitingSync = true;

    decodeProgress = seekTargetPercent;
    decodeProgressMs = seekTargetMs;
    playbackBaseMs = seekTargetMs;
    playbackTimer.restart();

    emit seekRequested(seekTargetPercent);

    if (isPlaying && firstFrameReceived) {
        progressTimer->start();
    }
}

void MainWindow::onSliderReleased()
{
    commitSliderSeek();
}
