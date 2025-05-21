#include "SVACDecoder.h"



SVACDecoder::SVACDecoder(int threadNum) {
	ZXSVAC_Initializer::Instance();
	this->_threadNum = threadNum;
}

SVACDecoder::~SVACDecoder() {
	if (_tmpYUVBuf) {
		free(_tmpYUVBuf);
		//_tmpYUVBuf = NULL;
	}
	if (_isGotParam) {
		TRI_SVAC_DECODE_UNINIT(&_vdCtx);
	}
}


bool SVACDecoder::inputFrame(const Frame& frame) {

	if (frame.keyFrame && !_isGotParam) {
		int ret = TRI_SVAC_DECODE_INIT(&_vdCtx, (unsigned char*)frame.data.data(), frame.size, I_frame, _threadNum);
		if (ret == 0) {
			_isGotParam = true;
			_tmpYUVBuf = (unsigned char*)malloc(static_cast<size_t>(_vdCtx.svac_param.width) * _vdCtx.svac_param.height * 5);
    //		qDebug() << "解码器初始化成功，视频尺寸:" << _vdCtx.svac_param.width << "x" << _vdCtx.svac_param.height;
		}
	}
	if (_isGotParam) {
		if (this->_cb) {
			ULONG outSize;
			Frame outFrame;
			outFrame.width = _vdCtx.svac_param.width;
			outFrame.height = _vdCtx.svac_param.height;
			TRI_SVAC_DECODE(&_vdCtx, (unsigned char*)frame.data.data(), frame.size, _tmpYUVBuf, &outSize, outFrame.width, outFrame.height);
			outFrame.data = std::vector<uint8_t>(_tmpYUVBuf, _tmpYUVBuf + outSize);
			outFrame.size = outSize;
			_cb(outFrame);
		}
		
	}

	return true;
	
}

void SVACDecoder::setOnDecode(onDec cb) {
	this->_cb = std::move(cb);
}
