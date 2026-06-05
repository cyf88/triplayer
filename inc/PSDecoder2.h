#ifndef PSDECODER2_H
#define PSDECODER2_H

#include "StreamDecoder.h"
#include <atomic>
#include <cstdint>

struct ps_demuxer_t;

class PSDecoder2 : public StreamDecoder {

public:

    using StreamDecoder::StreamDecoder;
    ~PSDecoder2();

    void parse() override;
    void setPosition(int pos) override;
    bool scanTimestampIndex() override;

private:

    static int onPsPacket(void* param, int stream, int codecid, int flags,
                          int64_t pts, int64_t dts, const void* data, size_t bytes);
    static int onPsPacketIndex(void* param, int stream, int codecid, int flags,
                               int64_t pts, int64_t dts, const void* data, size_t bytes);
    static void onPsStream(void* param, int stream, int codecid,
                           const void* extra, int bytes, int finish);

    void emitVideoFrame(const uint8_t* data, size_t bytes, int flags, size_t offset,
                        int64_t pts, int64_t dts);
    int feedDemuxer(const uint8_t* data, size_t bytes);
    bool demuxFile(FILE* fin, ps_demuxer_t* demuxer, bool stopOnPause);
    size_t alignPsSeekOffset(size_t offset) const;

    ps_demuxer_t* m_demuxer = nullptr;
    int m_videoStreamId = -1;

    static constexpr size_t kPsBufLen = 1 * 1024 * 1024;
    uint8_t* m_readBuf = nullptr;
    size_t m_bufRemain = 0;
    size_t m_fileOffset = 0;

    FILE* m_fin = nullptr;
};

#endif
