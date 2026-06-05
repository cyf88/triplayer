#include "PSDecoder2.h"
#include "easylogging++.h"
#include "mpeg-ps.h"
#include "mpeg-proto.h"
#include "mpeg-util.h"

#include <QString>
#include <vector>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <io.h>
#endif

namespace {

bool seekFile(FILE* fp, int64_t offset)
{
#ifdef _WIN32
    return _fseeki64(fp, offset, SEEK_SET) == 0;
#else
    return fseeko(fp, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
}

} // namespace

PSDecoder2::~PSDecoder2() {
    if (m_demuxer) {
        ps_demuxer_destroy(m_demuxer);
        m_demuxer = nullptr;
    }
    delete[] m_readBuf;
    m_readBuf = nullptr;
    if (m_fin) {
        fclose(m_fin);
        m_fin = nullptr;
    }
}

void PSDecoder2::onPsStream(void* param, int stream, int codecid,
                            const void* /*extra*/, int /*bytes*/, int /*finish*/) {
    auto* self = static_cast<PSDecoder2*>(param);
    if (!mpeg_stream_type_video(codecid)) {
        return;
    }
    if (self->m_videoStreamId < 0) {
        self->m_videoStreamId = stream;
        LOG(DEBUG) << "PS video stream id=" << stream << " codecid=" << codecid;
    }
}

int PSDecoder2::onPsPacket(void* param, int /*stream*/, int codecid, int flags,
                           int64_t pts, int64_t dts, const void* data, size_t bytes) {
    auto* self = static_cast<PSDecoder2*>(param);
    if (self->_bStop || !data || bytes == 0) {
        return 0;
    }
    if (!mpeg_stream_type_video(codecid)) {
        return 0;
    }

    self->emitVideoFrame(static_cast<const uint8_t*>(data), bytes, flags, self->m_fileOffset, pts, dts);
    return 0;
}

int PSDecoder2::onPsPacketIndex(void* param, int /*stream*/, int codecid, int flags,
                                int64_t pts, int64_t dts, const void* /*data*/, size_t /*bytes*/) {
    auto* self = static_cast<PSDecoder2*>(param);
    if (!mpeg_stream_type_video(codecid)) {
        return 0;
    }
    if ((flags & MPEG_FLAG_IDR_FRAME) == 0) {
        return 0;
    }

    const uint64_t ptsMs = ptsToMs(pts, dts);
    if (ptsMs == 0 && pts <= 0 && dts <= 0) {
        return 0;
    }

    self->_timestampIndex.push_back({self->m_fileOffset, ptsMs});
    return 0;
}

void PSDecoder2::emitVideoFrame(const uint8_t* data, size_t bytes, int flags, size_t offset,
                                int64_t pts, int64_t dts) {
    Frame frame;
    frame.data.assign(data, data + bytes);
    frame.size = frame.data.size();
    frame.timestamp = ptsToMs(pts, dts);
    frame.offset = offset;

    if (!_frameNumMap.empty()) {
        frame.frameNum = resolveKeyFrameIndex(offset);
    }

    const bool isIdr = (flags & MPEG_FLAG_IDR_FRAME) != 0;
    if (isIdr) {
        frame.hasSPS = true;
    }
    _frameNum++;

    if (_onOriginFrame) {
        _onOriginFrame(frame);
    }
    if (!_onFrame) {
        return;
    }

    if (!_decryptChannel) {
        _decryptChannel = SDKManager::Instance().createChannel(_streamId, ChannelType::DecryptChannel);
        if (!_decryptChannel) {
            return;
        }
    }
    Frame decFrame;
    _decryptChannel->doWork(frame, decFrame);
    if (!_svacDecoder) {
        _svacDecoder = std::make_shared<SVACDecoder>(0);
        _svacDecoder->setOnDecode([&](const Frame& f) {
            _onFrame(f);
        });
    }
    _svacDecoder->inputFrame(decFrame);
}

int PSDecoder2::feedDemuxer(const uint8_t* data, size_t bytes) {
    if (!m_demuxer || bytes == 0) {
        return 0;
    }
    const int consumed = ps_demuxer_input(m_demuxer, data, bytes);
    if (consumed < 0) {
        LOG(WARNING) << "ps_demuxer_input error, consumed=" << consumed;
        return -1;
    }
    return consumed;
}

bool PSDecoder2::demuxFile(FILE* fin, ps_demuxer_t* demuxer, bool stopOnPause) {
    m_bufRemain = 0;

    while (!feof(fin) && !_bStop) {
        if (stopOnPause && _bPause) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        const size_t space = kPsBufLen - m_bufRemain;
        if (space == 0) {
            LOG(WARNING) << "PS read buffer full, drop buffered data";
            m_bufRemain = 0;
            continue;
        }

        const size_t readlen = fread(m_readBuf + m_bufRemain, 1, space, fin);
        if (readlen == 0) {
            while (m_bufRemain > 0 && !_bStop) {
                const int r = feedDemuxer(m_readBuf, m_bufRemain);
                if (r <= 0) {
                    break;
                }
                memmove(m_readBuf, m_readBuf + r, m_bufRemain - static_cast<size_t>(r));
                m_bufRemain -= static_cast<size_t>(r);
                m_fileOffset += static_cast<size_t>(r);
            }
            break;
        }

        m_bufRemain += readlen;
        m_fileOffset = static_cast<size_t>(ftell(fin)) - m_bufRemain;

        while (m_bufRemain > 0 && !_bStop) {
            if (stopOnPause && _bPause) {
                break;
            }
            const int r = feedDemuxer(m_readBuf, m_bufRemain);
            if (r < 0) {
                return false;
            }
            if (r == 0) {
                break;
            }
            if (static_cast<size_t>(r) < m_bufRemain) {
                memmove(m_readBuf, m_readBuf + r, m_bufRemain - static_cast<size_t>(r));
            }
            m_bufRemain -= static_cast<size_t>(r);
        }
    }

    return true;
}

size_t PSDecoder2::alignPsSeekOffset(size_t offset) const
{
    if (!m_fin || _fileSize == 0) {
        return offset;
    }

    const size_t searchBack = 8192;
    const size_t start = offset > searchBack ? offset - searchBack : 0;
    const size_t readLen = offset - start + 4;
    if (readLen < 4) {
        return offset;
    }

    std::vector<uint8_t> buf(readLen);
    const long savedPos = ftell(m_fin);
    if (!seekFile(m_fin, static_cast<int64_t>(start))) {
        return offset;
    }
    if (fread(buf.data(), 1, readLen, m_fin) != readLen) {
        seekFile(m_fin, savedPos);
        return offset;
    }
    seekFile(m_fin, savedPos);

    size_t best = offset;
    for (size_t i = 0; i + 3 < readLen; ++i) {
        if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x01 && buf[i + 3] == 0xBA) {
            const size_t packStart = start + i;
            if (packStart <= offset) {
                best = packStart;
            }
        }
    }
    return best;
}

bool PSDecoder2::scanTimestampIndex() {
    _timestampIndex.clear();
    _frameNumMap.clear();
    _biFrameNumMap.clear();
    _startPtsMs = 0;
    _durationMs = 0;

    m_videoStreamId = -1;
    m_fileOffset = 0;

    if (!m_readBuf) {
        m_readBuf = new uint8_t[kPsBufLen];
    }

    QString qstr = QString::fromUtf8(_filePath);
    FILE* fin = _wfopen(reinterpret_cast<const wchar_t*>(qstr.utf16()), L"rb");
    if (!fin) {
        LOG(DEBUG) << "Cannot open input file for timestamp scan " << _filePath;
        return false;
    }

    fseek(fin, 0, SEEK_END);
    _fileSize = static_cast<size_t>(ftell(fin));
    fseek(fin, 0, SEEK_SET);

    static ps_demuxer_notify_t notify = {onPsStream};
    ps_demuxer_t* demuxer = ps_demuxer_create(onPsPacketIndex, this);
    ps_demuxer_set_notify(demuxer, &notify, this);
    m_demuxer = demuxer;

    demuxFile(fin, demuxer, false);

    ps_demuxer_destroy(demuxer);
    m_demuxer = nullptr;
    fclose(fin);

    if (_timestampIndex.empty()) {
        LOG(INFO) << "No timestamp index built for " << _filePath;
        return false;
    }

    finalizeTimestampIndex();
    LOG(INFO) << _filePath << " timestamp index size=" << _timestampIndex.size()
              << " durationMs=" << _durationMs;
    return true;
}

void PSDecoder2::parse() {
    m_videoStreamId = -1;
    m_bufRemain = 0;
    m_fileOffset = 0;

    if (!m_readBuf) {
        m_readBuf = new uint8_t[kPsBufLen];
    }

    QString qstr = QString::fromUtf8(_filePath);
    m_fin = _wfopen(reinterpret_cast<const wchar_t*>(qstr.utf16()), L"rb");
    if (!m_fin) {
        LOG(DEBUG) << "Cannot open input file " << _filePath;
        return;
    }

    fseek(m_fin, 0, SEEK_END);
    _fileSize = static_cast<size_t>(ftell(m_fin));
    fseek(m_fin, 0, SEEK_SET);

    static ps_demuxer_notify_t notify = {onPsStream};
    m_demuxer = ps_demuxer_create(onPsPacket, this);
    ps_demuxer_set_notify(m_demuxer, &notify, this);

    while (!_bStop) {
        size_t seekOffset = 0;
        if (consumeSeekRequest(seekOffset)) {
            m_videoStreamId = -1;
            if (m_demuxer) {
                ps_demuxer_destroy(m_demuxer);
            }
            m_demuxer = ps_demuxer_create(onPsPacket, this);
            ps_demuxer_set_notify(m_demuxer, &notify, this);
            if (m_fin) {
                seekOffset = alignPsSeekOffset(seekOffset);
                if (!seekFile(m_fin, static_cast<int64_t>(seekOffset))) {
                    LOG(WARNING) << "PS seek failed, offset=" << seekOffset;
                }
                m_fileOffset = seekOffset;
                m_bufRemain = 0;
                LOG(INFO) << "PS seek to offset=" << seekOffset
                          << " percent=" << _pendingSeekPercent.load();
            }
        }

        if (_bPause) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        const size_t space = kPsBufLen - m_bufRemain;
        if (space == 0) {
            LOG(WARNING) << "PS read buffer full, drop buffered data";
            m_bufRemain = 0;
            continue;
        }

        const size_t readlen = fread(m_readBuf + m_bufRemain, 1, space, m_fin);
        if (readlen == 0) {
            while (m_bufRemain > 0 && !_bStop) {
                const int r = feedDemuxer(m_readBuf, m_bufRemain);
                if (r <= 0) {
                    break;
                }
                memmove(m_readBuf, m_readBuf + r, m_bufRemain - static_cast<size_t>(r));
                m_bufRemain -= static_cast<size_t>(r);
                m_fileOffset += static_cast<size_t>(r);
            }
            break;
        }

        m_bufRemain += readlen;
        m_fileOffset = static_cast<size_t>(ftell(m_fin)) - m_bufRemain;

        while (m_bufRemain > 0 && !_bStop) {
            if (_needSeek.load()) {
                break;
            }
            if (_bPause) {
                break;
            }
            const int r = feedDemuxer(m_readBuf, m_bufRemain);
            if (r < 0) {
                break;
            }
            if (r == 0) {
                break;
            }
            if (static_cast<size_t>(r) < m_bufRemain) {
                memmove(m_readBuf, m_readBuf + r, m_bufRemain - static_cast<size_t>(r));
            }
            m_bufRemain -= static_cast<size_t>(r);
        }

        if (!_bPause && _seekReadBudget.load() > 0) {
            _seekReadBudget--;
        }
    }

    if (m_demuxer) {
        ps_demuxer_destroy(m_demuxer);
        m_demuxer = nullptr;
    }
    if (m_fin) {
        fclose(m_fin);
        m_fin = nullptr;
    }
}

void PSDecoder2::setPosition(int pos) {
    requestSeek(pos);
}
