#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QSlider>
#include <QTimer>
#include <QElapsedTimer>
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
    void receiveYUVFrame(const uchar* yData, const uchar* uData, const uchar* vData,
                        int ySize, int uSize, int vSize, int days, int counts);

    void moveSlider(int value);

    void setMediaInfo(qint64 durationMs, qint64 fileSizeBytes);

    void syncPlaybackProgress(qint64 progressMs);

    void setDecryptionFinished();

signals:
    void startDecryption();
    void pauseDecryption();
    void stopDecryption();
    void fileSelected(const QString& filePath);
    void seekRequested(int position);

private slots:
    void onPlayButtonClicked();
    void onPause();
    void onStop();
    void onExportButtonClicked();
    void onSliderPressed();
    void onSliderReleased();
    void updateSmoothProgress();

    void setProgressImmediate(int percent);
    void resetProgressSmooth();
    void beginPlaybackClock(int startPercent);
    void pausePlaybackClock();
    void beginSliderInteraction();
    void commitSliderSeek();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool isClickOnSliderHandle(const QPoint& pos) const;
    static int percentToSlider(int percent);
    static int sliderToPercent(int sliderValue);
    static int msToSlider(qint64 ms, qint64 durationMs);
    static qint64 sliderToMs(int sliderValue, qint64 durationMs);
    bool isSliderInteracting() const;

    YUVPlayer *yuvPlayer;
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *buttonLayout;
    
    QPushButton *playButton;
    QPushButton *pauseButton;
    QPushButton *stopButton;
    QPushButton *exportButton;

    QSlider *progressSlider;
    QTimer *progressTimer;
    QElapsedTimer playbackTimer;

    qint64 baseDurationMs = 0;
    qint64 totalDurationMs = 0;
    qint64 playbackBaseMs = 0;
    qint64 decodeProgressMs = 0;
    int decodeProgress = 0;
    int seekTargetPercent = 0;
    int seekTargetSlider = 0;
    qint64 seekStartMs = 0;
    qint64 seekTargetMs = 0;
    bool seekAwaitingSync = false;

    bool isPlaying;
    bool firstFrameReceived = false;
    bool sliderDragging = false;
    bool pressOnHandle = false;
    QString currentFile;
    
    SVACStreamDecoder *decoder;

    static constexpr int kSliderMax = 1000;
};

#endif // MAINWINDOW_H
