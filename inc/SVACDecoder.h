#ifndef SVACDECODER_H
#define SVACDECODER_H

#include "tri_svac.h"
#include "defs.h"
#include <functional>
#include <memory>

class ZXSVAC_Initializer {
public:
    static void Instance() {
        static ZXSVAC_Initializer instance;
    }

private:
    ZXSVAC_Initializer() {
        TRI_SVAC_INIT();
    }
    ~ZXSVAC_Initializer() {
        TRI_SVAC_DESTORY();
    }
    

    // 禁用拷贝和赋值
    ZXSVAC_Initializer(const ZXSVAC_Initializer&) = delete;
    ZXSVAC_Initializer& operator=(const ZXSVAC_Initializer&) = delete;
};

class SVACDecoder {

public:

    using Ptr = std::shared_ptr<SVACDecoder>;

    SVACDecoder(int threadNum);
    ~SVACDecoder();

    using onDec = std::function<void(const Frame& frame)>;

    bool inputFrame(const Frame& frame);

    void setOnDecode(onDec cb);

private:
    int _threadNum;
    
    bool _isGotParam = false;

    onDec _cb;

    VideoDecodeCtx _vdCtx;

    unsigned char* _tmpYUVBuf;
};








#endif