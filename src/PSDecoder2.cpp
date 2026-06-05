#include "PSDecoder2.h"
#include "easylogging++.h"
#include "mpeg-ps.h"
#include "mpeg-proto.h"
#include "mpeg-util.h"

#include <QString>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

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
                           int64_t /*pts*/, int64_t /*dts*/, const void* data, size_t bytes) {
    auto* self = static_cast<PSDecoder2*>(param);
    if (self->_bStop || !data || bytes == 0) {
        return 0;
    }
    if (!mpeg_stream_type_video(codecid)) {
        return 0;
    }

    self->emitVideoFrame(static_cast<const uint8_t*>(data), bytes, flags, self->m_fileOffset);
    return 0;
}

void PSDecoder2::emitVideoFrame(const uint8_t* data, size_t bytes, int flags, size_t offset) {
    Frame frame;
    frame.data.assign(data, data + bytes);
    frame.size = frame.data.size();
    frame.timestamp = 0;

    const bool isIdr = (flags & MPEG_FLAG_IDR_FRAME) != 0;
    if (isIdr) {
        frame.hasSPS = true;
        frame.offset = offset + 170;
        try {
            frame.frameNum = _frameNumMap.at(frame.offset);
            std::cout << " es frameNum: " << frame.frameNum << std::endl;
        } catch (const std::out_of_range&) {
            std::cout << "Key not found" << std::endl;
        }
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

    static ps_demuxer_notify_t notify = {onPsStream};
    m_demuxer = ps_demuxer_create(onPsPacket, this);
    ps_demuxer_set_notify(m_demuxer, &notify, this);

    while (!_bStop) {
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
    (void)pos;
}
