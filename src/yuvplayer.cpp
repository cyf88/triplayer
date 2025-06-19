#include "yuvplayer.h"
#include <QDebug>
#include <QCoreApplication>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>

// 顶点着色器
const char *vertexShaderSource =
    "attribute vec4 vertex;\n"
    "attribute vec2 texCoord;\n"
    "varying vec2 texCoordVar;\n"
    "void main() {\n"
    "    gl_Position = vertex;\n"
    "    texCoordVar = texCoord;\n"
    "}\n";

// 片段着色器
const char *fragmentShaderSource =
    "varying vec2 texCoordVar;\n"
    "uniform sampler2D textureY;\n"
    "uniform sampler2D textureU;\n"
    "uniform sampler2D textureV;\n"
    "void main() {\n"
    "    vec3 yuv;\n"
    "    vec3 rgb;\n"
    "    yuv.x = texture2D(textureY, texCoordVar).r - 0.0625;\n"
    "    yuv.y = texture2D(textureU, texCoordVar).r - 0.5;\n"
    "    yuv.z = texture2D(textureV, texCoordVar).r - 0.5;\n"
    "    rgb = mat3(1.164, 1.164, 1.164,\n"
    "               0.0, -0.391, 2.018,\n"
    "               1.596, -0.813, 0.0) * yuv;\n"
    "    gl_FragColor = vec4(rgb, 1.0);\n"
    "}\n";

YUVPlayer::YUVPlayer(QWidget *parent)
    : QOpenGLWidget(parent)
    , program(nullptr)
    , frameWidth(0)
    , frameHeight(0)
    , frameAvailable(false)
{
    // 设置自动更新
    setAutoFillBackground(false);
    
    // 设置最小尺寸
    setMinimumSize(320, 240);

    // 初始化文本字体
    textFont.setFamily("Microsoft YaHei");
    textFont.setPointSize(12);
    textFont.setBold(true);
}

YUVPlayer::~YUVPlayer()
{
    makeCurrent();
    delete program;
    glDeleteTextures(1, &textureY);
    glDeleteTextures(1, &textureU);
    glDeleteTextures(1, &textureV);
    doneCurrent();
}

void YUVPlayer::initializeGL()
{
    initializeOpenGLFunctions();
    
    // 创建纹理
    glGenTextures(1, &textureY);
    glGenTextures(1, &textureU);
    glGenTextures(1, &textureV);
    
    // 初始化纹理参数
    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, textureU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, textureV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    initShaders();
    
    // 设置清除颜色为黑色
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void YUVPlayer::initShaders()
{
    program = new QOpenGLShaderProgram;
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    program->link();
    
    program->bind();
    program->setUniformValue("textureY", 0);
    program->setUniformValue("textureU", 1);
    program->setUniformValue("textureV", 2);
}

void YUVPlayer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void YUVPlayer::paintGL()
{
    if (!frameAvailable) {
        qDebug() << "No frame available for rendering";
        return;
    }
    
    // 清除颜色缓冲
    glClear(GL_COLOR_BUFFER_BIT);
    
    const GLfloat vertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, 1.0f
    };
    
    const GLfloat textureCoords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
    };
    
    program->bind();
    
    // 设置顶点属性
    program->enableAttributeArray("vertex");
    program->enableAttributeArray("texCoord");
    program->setAttributeArray("vertex", vertices, 2);
    program->setAttributeArray("texCoord", textureCoords, 2);
    
    // Y分量
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frameWidth, frameHeight, 0, GL_RED, GL_UNSIGNED_BYTE, yBuffer.constData());
    
    // U分量
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureU);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frameWidth/2, frameHeight/2, 0, GL_RED, GL_UNSIGNED_BYTE, uBuffer.constData());
    
    // V分量
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textureV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frameWidth/2, frameHeight/2, 0, GL_RED, GL_UNSIGNED_BYTE, vBuffer.constData());
    
    // 绘制
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    // 清理
    program->disableAttributeArray("vertex");
    program->disableAttributeArray("texCoord");
    program->release();
    
    // 检查OpenGL错误
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        qDebug() << "OpenGL错误:" << err;
    }

    // 使用QPainter绘制文本
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(textFont);
    
    // 设置文本颜色和背景
    QPen pen(Qt::white);
    pen.setWidth(2);
    painter.setPen(pen);
    
    // 计算文本位置
    int lineHeight = painter.fontMetrics().height();
    int bottomMargin = 10;
    int leftMargin = 10;
    
    // 绘制第一行文本
    if (!displayTextLine1.isEmpty()) {
        // 绘制阴影
        QPainterPath path1;
        path1.addText(leftMargin + 2, height() - bottomMargin - lineHeight + 2, textFont, displayTextLine1);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawPath(path1);
        
        // 绘制文本
        painter.setPen(pen);
        painter.drawText(leftMargin, height() - bottomMargin - lineHeight, displayTextLine1);
    }
    
    // 绘制第二行文本
    if (!displayTextLine2.isEmpty()) {
        // 绘制阴影
        QPainterPath path2;
        path2.addText(leftMargin + 2, height() - bottomMargin + 2, textFont, displayTextLine2);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawPath(path2);
        
        // 绘制文本
        painter.setPen(pen);
        painter.drawText(leftMargin, height() - bottomMargin, displayTextLine2);
    }
}

void YUVPlayer::setFrameSize(int width, int height)
{
    frameWidth = width;
    frameHeight = height;
}

void YUVPlayer::updateYUVFrame(const uchar* yData, const uchar* uData, const uchar* vData,
                              int ySize, int uSize, int vSize)
{
    // 检查数据有效性
    if (!yData || !uData || !vData) {
        qDebug() << "YUV数据指针无效";
        return;
    }


    // qDebug() << "更新YUV帧 - 尺寸:" << frameWidth << "x" << frameHeight 
    //          << "Y大小:" << ySize << "U大小:" << uSize << "V大小:" << vSize;

    // 检查数据大小是否正确
    if (ySize != frameWidth * frameHeight ||
        uSize != frameWidth * frameHeight / 4 ||
        vSize != frameWidth * frameHeight / 4) {
        qDebug() << "YUV数据大小不正确:" << ySize << uSize << vSize 
                 << "期望大小:" << frameWidth * frameHeight 
                 << frameWidth * frameHeight / 4 
                 << frameWidth * frameHeight / 4;
        return;
    }
    
    // 更新YUV数据缓冲
    yBuffer = QByteArray(reinterpret_cast<const char*>(yData), ySize);
    uBuffer = QByteArray(reinterpret_cast<const char*>(uData), uSize);
    vBuffer = QByteArray(reinterpret_cast<const char*>(vData), vSize);
    
    frameAvailable = true;
    
    // 直接调用 update
    //update();
    repaint();
}

void YUVPlayer::setDisplayText(const QString& line1, const QString& line2)
{
    displayTextLine1 = line1;
    displayTextLine2 = line2;
    update(); // 触发重绘
} 
