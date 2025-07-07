#ifndef ESDECODER_H
#define ESDECODER_H

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#include "defs.h"
#include "SVACDecoder.h"
#include "SDKManager.h"
#include "StreamDecoder.h"



class ESDecoder : public StreamDecoder {

public:
    using StreamDecoder::StreamDecoder;

	void parse() override;
    void setPosition(int pos) override;

private:

    int _frameNum = 0;

    // 缓存区大小
    size_t _bufferSize = 1024*1024;


    // 获取文件大小
    size_t getFileSize(std::ifstream& file);

    // 解析读取的文件块
    void parseBlock(const std::vector<uint8_t>& buffer, size_t bytesRead, size_t& startPos, size_t curFilePos);

    // 查找NAL单元的起始码
    size_t findNALStartCode(const std::vector<uint8_t>& buffer, size_t startPos, size_t bufferEnd);


};











#endif
