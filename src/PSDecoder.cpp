#include "PSDecoder.h"
#include "easylogging++.h"
#include <QString>
#include <algorithm>
#include <cstring>

PSDecoder::~PSDecoder() {
	if (m_psbuf != NULL) {
		delete m_psbuf;
	}
	if (m_pPSbuf != NULL) {
		delete m_pPSbuf;
	}
	if (fin != NULL) {
		fclose(fin);
	}
}

void PSDecoder::resetSession() {
	m_PsBufLen = 0;
	m_pendingPesBytes = 0;
	m_psAudioType = 0;
	m_psStreamType = 0;
}

unsigned int PSDecoder::readLe32(const unsigned char* p) {
	return static_cast<unsigned int>(p[0])
	       | (static_cast<unsigned int>(p[1]) << 8)
	       | (static_cast<unsigned int>(p[2]) << 16)
	       | (static_cast<unsigned int>(p[3]) << 24);
}

void PSDecoder::appendEs(const unsigned char* data, int len) {
	if (len <= 0 || data == nullptr) {
		return;
	}
	if (m_PsBufLen + len > ES_BUFLEN) {
		LOG(WARNING) << "PS ES buffer overflow, drop " << len << " bytes";
		return;
	}
	memcpy(m_psbuf + m_PsBufLen, data, static_cast<size_t>(len));
	m_PsBufLen += len;
}

void PSDecoder::consumeVideoPes(unsigned char*& pCurBuf, unsigned char* packEnd) {
	if (pCurBuf + 9 > packEnd) {
		return;
	}

	const int pes_packet_length = (pCurBuf[4] << 8) | pCurBuf[5];
	const int pes_header_data_length = pCurBuf[8];
	if (pCurBuf + 9 + pes_header_data_length > packEnd) {
		return;
	}

	unsigned char* payload = pCurBuf + 9 + pes_header_data_length;
	int payload_length = 0;
	if (pes_packet_length == 0) {
		payload_length = static_cast<int>(packEnd - payload);
	} else {
		payload_length = pes_packet_length - 3 - pes_header_data_length;
	}
	if (payload_length <= 0) {
		pCurBuf = packEnd;
		return;
	}

	if (m_pendingPesBytes == 0) {
		m_pendingPesBytes = payload_length;
	}

	const int avail = static_cast<int>(packEnd - payload);
	const int copy = std::min(m_pendingPesBytes, avail);
	appendEs(payload, copy);
	m_pendingPesBytes -= copy;
	pCurBuf = payload + copy;
}

void PSDecoder::skipAudioPes(unsigned char*& pCurBuf, unsigned char* packEnd) {
	if (pCurBuf + 9 > packEnd) {
		pCurBuf = packEnd;
		return;
	}

	const int pes_packet_length = (pCurBuf[4] << 8) | pCurBuf[5];
	const int pes_header_data_length = pCurBuf[8];
	unsigned char* payload = pCurBuf + 9 + pes_header_data_length;

	int payload_length = 0;
	if (pes_packet_length == 0) {
		payload_length = static_cast<int>(packEnd - payload);
	} else {
		payload_length = pes_packet_length - 3 - pes_header_data_length;
	}
	if (payload_length < 0) {
		pCurBuf = packEnd;
		return;
	}

	const int avail = static_cast<int>(packEnd - payload);
	const int skip = std::min(payload_length, avail);
	pCurBuf = payload + skip;
}

void PSDecoder::parseProgramStreamMap(unsigned char* pCurBuf, int psmlength) {
	UINT32 u32PSILength = (static_cast<UINT32>(pCurBuf[8]) << 8) | pCurBuf[9];

	UINT32 u32ESMLength = 0;
	if (10 + u32PSILength + 1 < static_cast<unsigned>(psmlength + 6)) {
		u32ESMLength = (static_cast<UINT32>(pCurBuf[10 + u32PSILength]) << 8)
		               | pCurBuf[11 + u32PSILength];
	}

	UINT8* pESM = pCurBuf + 12 + u32PSILength;
	UINT32 u32Tmp = 0;
	while (u32Tmp < u32ESMLength) {
		UINT32 u32StreamID = pESM[1 + u32Tmp];
		u32StreamID &= 0x000000FF;
		u32StreamID |= 0x00000100;
		if (u32StreamID == PS_PES_AUDIO) {
			m_psAudioType = pESM[0 + u32Tmp];
		} else if (u32StreamID == PS_PES_VIDEO) {
			m_psStreamType = pESM[0 + u32Tmp] & 0xFF;
			switch (m_psStreamType) {
			case _Stream_MPEG4:
				LOG(DEBUG) << "Stream_MPEG4";
				break;
			case _Stream_H265:
				LOG(DEBUG) << "Stream_H265";
				break;
			case _Stream_H264:
				LOG(DEBUG) << "Stream_H264";
				break;
			case _Stream_SVAC_Video:
				break;
			default:
				break;
			}
		}

		UINT32 u32ESILength = (static_cast<UINT32>(pESM[2 + u32Tmp]) << 8)
		                      | pESM[3 + u32Tmp];
		if (u32ESILength == 0) {
			break;
		}
		u32Tmp += (4 + u32ESILength);
	}
}

void PSDecoder::emitAccumulatedFrame(int frametype, size_t offset) {
	if (m_PsBufLen == 0) {
		return;
	}

	Frame frame;
	frame.data = std::vector<uint8_t>(m_psbuf, m_psbuf + m_PsBufLen);
	frame.size = frame.data.size();
	frame.timestamp = 0;
	if (frametype == I_frame) {
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
	if (_onFrame) {
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

	m_PsBufLen = 0;
}

void PSDecoder::flushRemainingEs(int frametype, size_t offset) {
	if (m_PsBufLen == 0) {
		m_pendingPesBytes = 0;
		return;
	}
	emitAccumulatedFrame(frametype, offset);
	m_pendingPesBytes = 0;
}

void PSDecoder::parse() {
	resetSession();
	int buflen = 0;
	unsigned char* buf = m_pPSbuf;
	int lastFrametype = P_frame;

	QString qstr = QString::fromUtf8(_filePath);
	fin = _wfopen(reinterpret_cast<const wchar_t*>(qstr.utf16()), L"rb");
	if (fin == 0) {
		LOG(DEBUG) << "Cann't open input file " << _filePath;
		return;
	}
	long pos = 0;
	while (!_bStop) {
		int readlen = static_cast<int>(fread(m_pPSbuf + buflen, 1, PSBUFLEN - buflen, fin));
		if (readlen <= 0) {
			if (buflen > 0) {
				DecodePSPacket(m_pPSbuf, static_cast<unsigned int>(buflen), static_cast<size_t>(pos));
			}
			flushRemainingEs(lastFrametype, static_cast<size_t>(pos));
			break;
		}
		readlen += buflen;
		buflen = 0;
		buf = m_pPSbuf;

		while (!_bStop) {
			if (_bPause) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
			int iRvCurren = ResearchPSTag(buf + buflen, readlen - buflen);
			if (iRvCurren >= 0) {
				buflen += iRvCurren;
			} else {
				buflen = 0;
				break;
			}
			int iRvNext = ResearchPSTag(buf + buflen + 4, readlen - buflen - 4);
			if (iRvNext == 0) {
				iRvNext += 4;
				buflen += iRvNext;
				continue;
			} else if (iRvNext > 0) {
				iRvNext += 4;
				lastFrametype = DecodePSPacket(buf + buflen, static_cast<unsigned int>(iRvNext),
				                               static_cast<size_t>(pos + buflen));
				buflen += iRvNext;
				continue;
			} else if (iRvNext < 0) {
				buf += buflen;
				buflen = readlen - buflen;
				memcpy(m_pPSbuf, buf, static_cast<size_t>(buflen));
				break;
			}
		}
		pos = static_cast<long>(ftell(fin) - buflen);
	}
	if (fin != nullptr) {
		fclose(fin);
		fin = nullptr;
	}
}

int PSDecoder::ResearchPSTag(unsigned char* buf, unsigned int len) {
	unsigned int code = 0xFFFFFFFF;
	unsigned char* pos = buf;
	int rest = static_cast<int>(len);
	int ret = -1;
	while (rest-- > 0) {
		code = (code << 8) | *pos++;
		if (code == 0x01BA) {
			ret = static_cast<int>(len) - rest - 4;
			break;
		}
	}
	return ret;
}

int PSDecoder::DecodePSPacket(unsigned char* pBuf, unsigned int size, size_t offset) {
	unsigned char* pCurBuf = pBuf;
	unsigned char* packEnd = pBuf + size;
	int frametype = P_frame;

	while (!_bStop && pCurBuf + 4 <= packEnd) {
		if (!(pCurBuf[0] == 0x00 && pCurBuf[1] == 0x00 && pCurBuf[2] == 0x01)) {
			break;
		}

		const unsigned int code = readLe32(pCurBuf);
		if (code == 0xBA010000) {
			frametype = P_frame;
			pCurBuf += 13;
			if (pCurBuf >= packEnd) {
				break;
			}
			const unsigned int pack_stuffing_length = *pCurBuf & 7u;
			pCurBuf += 1 + pack_stuffing_length;
			continue;
		}
		if (code == 0xBB010000) {
			if (pCurBuf + 6 > packEnd) {
				break;
			}
			const int systemheader_length = (pCurBuf[4] << 8) | pCurBuf[5];
			pCurBuf += systemheader_length + 6;
			frametype = I_frame;
			continue;
		}
		if (code == 0xBC010000) {
			if (pCurBuf + 6 > packEnd) {
				break;
			}
			const int psmlength = (pCurBuf[4] << 8) | pCurBuf[5];
			frametype = I_frame;
			parseProgramStreamMap(pCurBuf, psmlength);
			pCurBuf += psmlength + 6;
			continue;
		}
		if (code == 0xE0010000) {
			consumeVideoPes(pCurBuf, packEnd);
			continue;
		}
		if (code == 0xC0010000) {
			skipAudioPes(pCurBuf, packEnd);
			continue;
		}
		break;
	}

	// 当前 PES 已拼完才输出；否则保留 ES 与 pending 供下一 pack 续传
	if (m_PsBufLen != 0 && m_pendingPesBytes == 0) {
		emitAccumulatedFrame(frametype, offset);
	}

	return frametype;
}

void PSDecoder::setPosition(int pos) {
	(void)pos;
}
