
#ifndef STREAMDECODER_H
#define STREAMDECODER_H

#include "SVACDecoder.h"
#include "SDKManager.h"
#include "easylogging++.h"
#include <fstream>
#include <map>
#include <QString>

struct IDRFrame {
    size_t offset;  // 在文件中的偏移
    size_t frameIndex;
};
enum NaluType {
    NAL_NONE_IDR = 1,
    NAL_IDR = 2,
    NAL_NONE_IDR_SVC = 3,
    NAL_IDR_SVC = 4,
    NAL_SURVEI_UNIT = 5,
    NAL_SEI = 6,
    NAL_SPS = 7,
    NAL_PPS = 8,
    NAL_SES = 9,
    NAL_AUTH = 10,
    NAL_E0STREAM = 11,
    NAL_RESERVED1 = 12,
    NAL_CHAPTER6 = 13,
    NAL_RESERVED2 = 14,
    NAL_SVC_PPS = 15,
};

#define SVAC_TYPE(v) ((uint8_t)(v>>2)&0x0F)


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
    ~StreamDecoder() {}


    virtual void parse() = 0;

    virtual void setPosition(int pos) = 0;

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

    virtual size_t getKeyFrameSum() const final {
        return _frameNumMap.size();
    }

    //virtual bool getKeyFrameSum() = 0;

    // 获取播放权限
    virtual bool getPlayRightInfo(unsigned int& spandays, unsigned int& spanCount) const final {
       // 读取文件前1000字节
        std::ifstream file(_filePath, std::ios::binary);
        if (!file.is_open()) {
            LOG(INFO) << "Failed to open file for reading first 1000 bytes";
            return false;
        }

        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        int bufSize =  3*1024*1024;

        if (fileSize < bufSize) {
            bufSize = fileSize;
        }

        char* buff = (char*)malloc(bufSize);
        file.read(buff, bufSize);
        size_t bytesRead = file.gcount();
        file.close();
        LOG(INFO) << "Read first" << bytesRead << "bytes from file";
        if(SDKManager::Instance().queryPlayRightInfo((unsigned char*)buff, bytesRead, spandays, spanCount) ==0) {
            LOG(INFO) << "spandays:" << spandays << "spanCount:" << spanCount;
            return true;
        }
        return false;

    }

    virtual bool scanIDRs() {

        // QString qstr = QString::fromUtf8(_filePath);

        // std::wstring wpath = qstr.toStdWString();

        // QString qstr = QString::fromUtf8(_filePath);

        std::ifstream file(_filePath, std::ios::binary);
        if (!file.is_open()) {
            LOG(INFO) << "Failed to open file for scanIDRs: " << _filePath;
            return false;
        }

        const uint8_t startCode[] = {0x00, 0x00, 0x00, 0x01};
        std::vector<uint8_t> buffer(10 * 1024 * 1024); // 10MB 缓冲
        file.seekg(0);
        size_t offset = 0;
        size_t frameIdx = 0;

        while (!file.eof()) {
            file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            std::streamsize bytesRead = file.gcount();

            for (size_t i = 0; i + 4 < (size_t)bytesRead; ++i) {
                if (memcmp(&buffer[i], startCode, 4) == 0) {
                    uint8_t nalType = SVAC_TYPE(buffer[i + 4]);
                    if (nalType == NAL_SPS) {
                        _frameNumMap.emplace(offset+i, frameIdx++);
                        //_biFrameNumMap.emplace(frameIdx, offset+i);
                    }
                }
            }
            offset += bytesRead;
        }
        file.close();

        LOG(INFO) << _filePath <<" scanIDRs keyFrameNum: " << _frameNumMap.size();
    }




protected:

    int _frameNum = 0;

    // 文件路径
    std::string _filePath;
    std::string _streamId;

    //int _keyFrameSum;
    bool keyFrameSumFlag;


    // 回调函数
    OnFrame _onFrame;

    OnFrame _onOriginFrame;

    volatile bool _bStop = false;

    volatile bool _bPause = false;
    
    SVACDecoder::Ptr _svacDecoder;

    Channel::Ptr _decryptChannel;

    //关键帧在文件中的位置，为了实现播放拖动seek, <offset, frameNum>
    std::map<size_t, size_t> _frameNumMap;
    //反向map，用于快速定位 <frameNum, offset>
    std::map<size_t, size_t> _biFrameNumMap;

};

#endif
