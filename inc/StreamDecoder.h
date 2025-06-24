
#ifndef STREAMDECODER_H
#define STREAMDECODER_H

#include "SVACDecoder.h"
#include "SDKManager.h"
#include "easylogging++.h"
#include <fstream>


class StreamDecoder {

public:

	using OnFrame = std::function<void(const Frame& frame)>;

    StreamDecoder(const std::string filePath, const std::string streamId) {
        _filePath = filePath;
        _streamId = streamId;
    }
    StreamDecoder(const std::string filePath) {
        _filePath = filePath;
    }
    ~StreamDecoder() {};


    virtual void parse() = 0;

    //解码帧回调,不要子类实现
    virtual void setOnFrame(OnFrame onFrame) final {
        _onFrame = onFrame;
    }

    //原始帧回调
    virtual void setOnOrignFrame(OnFrame onFrame) final {
        _onOriginFrame = onFrame;
    }

    // 设置暂停标志
    virtual void setPause(bool pause) final {
        _bPause = pause;
    }

    // 获取暂停标志
    virtual bool getPause() const final {
        return _bPause;
    }

    // 设置停止标志
    virtual void setStop(bool stop) final {
        _bStop = stop;
    }

    // 获取停止标志
    virtual bool getStop() const final {
        return _bStop;
    }

    // 获取播放权限
    virtual bool getPlayRightInfo(unsigned int& spandays, unsigned int& spanCount) const final {
       // 读取文件前1000字节
        std::ifstream file(_filePath, std::ios::binary);
        if (!file.is_open()) {
            LOG(INFO) << "Failed to open file for reading first 1000 bytes";
            return false;
        }

        char buff [1000];
        file.read(buff, 1000);
        size_t bytesRead = file.gcount();
        file.close();
        LOG(INFO) << "Read first" << bytesRead << "bytes from file";
        if(SDKManager::Instance().queryPlayRightInfo((unsigned char*)buff, bytesRead, spandays, spanCount) ==0) {
            LOG(INFO) << "spandays:" << spandays << "spanCount:" << spanCount;
            return true;
        }
        return false;

    }


protected:

    int _frameNum = 0;

    // 文件路径
    std::string _filePath;
    std::string _streamId;


    // 回调函数
    OnFrame _onFrame;

    OnFrame _onOriginFrame;

    volatile bool _bStop = false;

    volatile bool _bPause = false;
    
    SVACDecoder::Ptr _svacDecoder;

    Channel::Ptr _decryptChannel;

};

#endif
