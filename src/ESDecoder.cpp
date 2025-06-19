#include "ESDecoder.h"
#include <fstream>
#include <iostream>
#include "SDKManager.h"
#include <thread>
#include <chrono>
#include <QThread>


size_t ESDecoder::getFileSize(std::ifstream& file) {
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    return size;
}

void ESDecoder::parse() {
    std::ifstream file(_filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << _filePath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(_bufferSize);
    size_t fileSize = getFileSize(file);

    while (file.tellg() < fileSize ) {
        // if (_bPause) {
        //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //     continue;
        // }
        // 读取一个块
        file.read(reinterpret_cast<char*>(buffer.data()), _bufferSize);
        size_t bytesRead = file.gcount();
        if (bytesRead == 0) {
            break;
        }

        // 处理当前读取的块
        size_t lastPos;
        parseBlock(buffer, bytesRead, lastPos);

        // 更新起始位置
        file.seekg(lastPos - _bufferSize, std::ios::cur);

    }
}


void ESDecoder::parseBlock(const std::vector<uint8_t>& buffer,
    size_t bytesRead, size_t& startPos) {

    size_t bufferPos = 0;
    size_t bufferEnd = bytesRead;

    size_t bufferSPS = 0;

    // 找到NAL单元
    while (bufferPos < bufferEnd) {
        // 查找NAL单元起始码（0x000001E0 或 0x00000001DC）
        size_t endPos = findNALStartCode(buffer, bufferPos, bufferEnd);
        if (endPos == std::string::npos) break;

        if (endPos > 4 ) {

            if (endPos - bufferPos < 20) {
                //是sps帧 ,需要记录位置，不分割sps
                bufferSPS = bufferPos;
            }
            else {

                if (bufferSPS != 0) {
                    //如果是SPS帧，那么分割是需要从bufferSPS位置开始
                    bufferPos = bufferSPS;
                }

                Frame frame;
                frame.data = std::vector<uint8_t>(buffer.begin() + bufferPos - 5, buffer.begin() + endPos - 4);
                frame.size = frame.data.size();
                frame.timestamp = 0; // 如果有时间戳信息，可以在这里填充
                _frameNum++;
                bufferSPS = 0;
                            
                
                if (_onOriginFrame) {
                    _onOriginFrame(frame);
                }
                //debug 
                //std::ofstream outfile;
                //outfile.open("E:\\code\\VSS\\sdk2\\bin\\x64-Debug\\Debug//es//es_frame_" + std::to_string(_frameNum) + ".es", std::ios::out | std::ios::binary | std::ios::trunc);
                //outfile.write((char*)frame.data.data(), frame.size);
                //outfile.close();

                // 如果设置了回调函数，调用回调
                if (_onFrame) {
                    //解密
                    //std::string chid = "13102800001310434532";
                    //std::string chid = "34020000001319000111";
                    if (!_decryptChannel) {
                        _decryptChannel = SDKManager::Instance().createChannel(_streamId, ChannelType::DecryptChannel);
                        if (!_decryptChannel) continue;
                    }
                    Frame decFrame;
                    _decryptChannel->doWork(frame, decFrame);
                    //debug 
                    //std::ofstream outfile;
                    //outfile.open("E://es//dec-sdk2//decrypt-" + std::to_string(_frameNum) + ".es", std::ios::out | std::ios::binary | std::ios::trunc);
                    //outfile.write((char*)decFrame.data.data(), decFrame.size);
                    //outfile.close();
                    if (!_svacDecoder) {
                        _svacDecoder = std::make_shared<SVACDecoder>(0);
                        _svacDecoder->setOnDecode([&](const Frame& frame) {
                            _onFrame(frame);
                            });
                    }
                    _svacDecoder->inputFrame(decFrame);

                }
            }


        }

        // 更新当前NAL单元的起始位置      
        bufferPos = endPos + 1;
    }
    startPos = bufferPos - 5;
}

size_t ESDecoder::findNALStartCode(const std::vector<uint8_t>& buffer, size_t startPos, size_t bufferEnd) {
    for (size_t i = startPos + 3; i < bufferEnd; i++) {
        if (buffer[i - 3] == 0x00 && buffer[i - 2] == 0x00 && 
            buffer[i - 1] == 0x00 && buffer[i] == 0x01) {
            // 检查起始码后是否有 0xdc (SPS)  或者 0xe0 (PPS), 
            //xinbo 码流每一帧都带有pps，
            if ((i + 1 < bufferEnd && buffer[i + 1] == 0xdc) 
                || (i + 1 < bufferEnd && buffer[i + 1] == 0xe0)) {
                return i + 1; // 返回 0xdc 的位置
            }
        }
    }
    return std::string::npos; // 没有找到起始码
}
