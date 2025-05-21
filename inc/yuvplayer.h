#ifndef YUVPLAYER_H
#define YUVPLAYER_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

class YUVPlayer : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit YUVPlayer(QWidget *parent = nullptr);
    ~YUVPlayer();

    // 设置帧大小
    void setFrameSize(int width, int height);
    
    // 接收新的YUV帧数据
    void updateYUVFrame(const uchar* yData, const uchar* uData, const uchar* vData,
                       int ySize, int uSize, int vSize);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    void initShaders();

    QOpenGLShaderProgram *program;
    GLuint textureY;
    GLuint textureU;
    GLuint textureV;
    
    int frameWidth;
    int frameHeight;
    
    // YUV数据缓冲
    QByteArray yBuffer;
    QByteArray uBuffer;
    QByteArray vBuffer;
    
    bool frameAvailable;
};

#endif // YUVPLAYER_H 