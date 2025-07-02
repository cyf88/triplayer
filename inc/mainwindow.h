#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QSlider>
#include "yuvplayer.h"
#include "svacstreamdecoder.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setVideoSize(int width, int height);

public slots:
    // 提供接口用于接收YUV数据
    void receiveYUVFrame(const uchar* yData, const uchar* uData, const uchar* vData,
                        int ySize, int uSize, int vSize, int days, int counts);

    void moveSlider(int value);

    void setDecryptionFinished();

signals:
    // 用于通知外部解密类的信号
    void startDecryption();
    void pauseDecryption();
    void stopDecryption();
    void fileSelected(const QString& filePath);

private slots:
    void onPlayButtonClicked();
    void onPause();
    void onStop();
    void onExportButtonClicked();

private:
    YUVPlayer *yuvPlayer;
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *buttonLayout;
    
    QPushButton *playButton;
    QPushButton *pauseButton;
    QPushButton *stopButton;
    QPushButton *exportButton;

    QSlider *progressSlider;

    bool isPlaying;
    QString currentFile;
    
    // 解密器
    SVACStreamDecoder *decoder;
};

#endif // MAINWINDOW_H 
