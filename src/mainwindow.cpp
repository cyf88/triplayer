#include "mainwindow.h"
#include <QMessageBox>
#include "svacstreamdecoder.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isPlaying(false)
{
    // 设置窗口标题和大小
    setWindowTitle("SVAC视频播放器");
    resize(800, 600);

    // 创建中央部件和布局
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    mainLayout = new QVBoxLayout(centralWidget);

    // 创建视频播放器组件
    yuvPlayer = new YUVPlayer(this);

    // 创建按钮布局
    buttonLayout = new QHBoxLayout();

    // 创建控制按钮
    playButton = new QPushButton("打开文件", this);
    pauseButton = new QPushButton("暂停", this);
    stopButton = new QPushButton("停止", this);

    // 添加按钮到水平布局
    buttonLayout->addWidget(playButton);
    buttonLayout->addWidget(pauseButton);
    buttonLayout->addWidget(stopButton);

    // 添加组件到主布局
    mainLayout->addWidget(yuvPlayer);
    mainLayout->addLayout(buttonLayout);

    // 创建解密器
    decoder = new SVACStreamDecoder(this, this);

    // 连接信号和槽
    connect(playButton, &QPushButton::clicked, this, &MainWindow::onPlayButtonClicked);
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStop);

    // 连接解密器相关的信号和槽
    // connect(this, &MainWindow::fileSelected, decoder, &SVACStreamDecoder::onFileSelected);
    // connect(this, &MainWindow::startDecryption, decoder, &SVACStreamDecoder::onStartDecryption);
    // connect(this, &MainWindow::pauseDecryption, decoder, &SVACStreamDecoder::onPauseDecryption);
    // connect(this, &MainWindow::stopDecryption, decoder, &SVACStreamDecoder::onStopDecryption);

    connect(this, &MainWindow::fileSelected, decoder, &SVACStreamDecoder::onFileSelected, Qt::QueuedConnection);
    connect(this, &MainWindow::startDecryption, decoder, &SVACStreamDecoder::onStartDecryption, Qt::DirectConnection);
    connect(this, &MainWindow::pauseDecryption, decoder, &SVACStreamDecoder::onPauseDecryption, Qt::DirectConnection);
    connect(this, &MainWindow::stopDecryption, decoder, &SVACStreamDecoder::onStopDecryption, Qt::DirectConnection);
    // 初始化按钮状态
    pauseButton->setEnabled(false);
    stopButton->setEnabled(false);
}

MainWindow::~MainWindow()
{
}

void MainWindow::receiveYUVFrame(const uchar* yData, const uchar* uData, const uchar* vData,
                                int ySize, int uSize, int vSize)
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
}

void MainWindow::setVideoSize(int width, int height)
{
    yuvPlayer->setFrameSize(width, height);
}

void MainWindow::onPlayButtonClicked()
{
    if (!isPlaying) {
        if (currentFile.isEmpty()) {
            // 如果没有打开文件，先打开文件
            QString fileName = QFileDialog::getOpenFileName(this,
                "打开SVAC文件", "", "SVAC文件 (*.ps *.es);;所有文件 (*.*)");

            if (fileName.isEmpty()) {
                return;  // 用户取消选择
            }

            currentFile = fileName;
            emit fileSelected(fileName);
        }

        // 开始播放
        isPlaying = true;
        emit startDecryption();
        
        playButton->setText("播放中...");
        pauseButton->setEnabled(true);
        stopButton->setEnabled(true);
    }
}

void MainWindow::onPause()
{
    if (isPlaying) {
        isPlaying = false;
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
    emit stopDecryption();
    currentFile.clear();  // 清除当前文件
    
    playButton->setText("打开文件");
    playButton->setEnabled(true);
    pauseButton->setEnabled(false);
    stopButton->setEnabled(false);
}
