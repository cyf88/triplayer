#ifndef PSDECODER_H
#define PSDECODER_H

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include "defs.h"
#include "StreamDecoder.h"


#define PSBUFLEN (1 * 1024 * 1024)
#define PS_PES_AUDIO					(0x000001C0)
#define PS_PES_VIDEO					(0x000001E0)

typedef enum _tagEmStreamType
{
    _Stream_Unknown,
    _Stream_MPEG4 = 0x10,
    _Stream_H264 = 0x1B,
    _Stream_H265 = 0x24,
    _Stream_SVAC_Video = 0x80,
    _Stream_G711 = 0x90,
    _Stream_G7221 = 0x92,
    _Stream_G7231 = 0x93,
    _Stream_G729 = 0x99,
    _Stream_SVAC_Audio = 0x9B,
} EmStreamType;



class PSDecoder : public StreamDecoder {

public:

    using StreamDecoder::StreamDecoder;
    ~PSDecoder();

    void parse() override;
    void setPosition(int pos) override;

private:

    int _frameNum = 0;


    void DecodePSPacket(unsigned char* pBuf, unsigned int size, size_t offset);
    int ResearchPSTag(unsigned char* buf, unsigned int len);

    FILE* fin;

    unsigned char m_psAudioType;
    unsigned char m_psStreamType;
    unsigned char* m_psbuf = new unsigned char[2 * 1024 * 1024];
    unsigned char* m_pPSbuf = new unsigned char[PSBUFLEN];



};











#endif
