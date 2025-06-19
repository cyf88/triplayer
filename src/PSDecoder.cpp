#include"PSDecoder.h"
#include "easylogging++.h"
#include <QString>

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

void PSDecoder::parse() {
	int buflen = 0;
	unsigned char* buf = m_pPSbuf;

    QString qstr = QString::fromUtf8(_filePath);

    //fin = fopen(strPath.c_str(), "rb");
    // fopen 不支持中文路径
    fin = _wfopen(reinterpret_cast<const wchar_t*>(qstr.utf16()), L"rb");
	if (fin == 0) {
		LOG(DEBUG) << "Cann't open input file " << _filePath;
		return ;
	}

	while (!_bStop) {
		int readlen = fread(m_pPSbuf + buflen, 1, PSBUFLEN - buflen, fin);
		if (readlen <= 0) {
			DecodePSPacket(m_pPSbuf, buflen);
			break;
		}
		readlen += buflen;
		buflen = 0;

		buf = m_pPSbuf;

		while (!_bStop)
		{
			if (_bPause) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
			int iRvCurren = ResearchPSTag(buf + buflen, readlen - buflen);			//查找第一个PS头的位置
			if (iRvCurren >= 0)
			{
				buflen += iRvCurren;
			}
			else
			{
				buflen = 0;
				break;
			}
			//unsigned long long dFrameTimeTest = timeGetTime();
			int iRvNext = ResearchPSTag(buf + buflen + 4, readlen - buflen - 4);		//查找第二个PS头的位置
			if (iRvNext == 0)
			{
				iRvNext += 4;
				buflen += iRvNext;
				continue;
			}
			else if (iRvNext > 0)
			{
				iRvNext += 4;
				DecodePSPacket(buf + buflen, iRvNext);							//解析出裸码流并解码
				buflen += iRvNext;
				continue;
			}
			else if (iRvNext < 0)
			{
				//						buflen = 0;
				buf += buflen;
				buflen = readlen - buflen;
				memcpy(m_pPSbuf, buf, buflen);
				break;
			}
		}
	}
}


int PSDecoder::ResearchPSTag(unsigned char* buf, unsigned int len) {
	unsigned int code = 0xFFFFFFFF;
	unsigned char* pos = buf;	// 拿到数据区首指针
	int rest = len;
	int ret = -1;
	while (rest--)
	{
		code = (code << 8) | *pos++;
		if (code == 0x01BA)
		{
			ret = len - rest - 4;
			break;
		}
	}
	return ret;
}

void PSDecoder::DecodePSPacket(unsigned char* pBuf, unsigned int size) {
	unsigned char* pCurBuf = pBuf;
	int m_PsBufLen = 0;
	int frametype = P_frame;
	while (!_bStop)
	{
		if (pCurBuf >= (pBuf + size))
		{
			break;
		}

		unsigned int* pcode = (unsigned int*)pCurBuf;
		if (*pcode == 0xBA010000)												//查找P帧类型
		{
			frametype = P_frame;
			pCurBuf += 13;
			unsigned int pack_stuffing_length = *pCurBuf & 7;
			pCurBuf++;
			pCurBuf += pack_stuffing_length;
			continue;
		}
		else if (*pcode == 0xBB010000)											//查找I帧类型
		{
			int Systemheader_length = *(pCurBuf + 4) * 256 + *(pCurBuf + 5);
			pCurBuf += Systemheader_length + 4 + 2;
			frametype = I_frame;
			continue;
		}
		else if (*pcode == 0xBC010000)											//查找I帧类型
		{
			int psmlength = *(pCurBuf + 4) * 256 + *(pCurBuf + 5);
			frametype = I_frame;

			UINT32 u32PSILength = 0;
			u32PSILength = u32PSILength | pCurBuf[8];
			u32PSILength = (u32PSILength << 8) | pCurBuf[9];

			UINT32 u32ESMLength = 0;
			u32ESMLength = u32ESMLength | pCurBuf[10 + u32PSILength];
			u32ESMLength = (u32ESMLength << 8) | pCurBuf[11 + u32PSILength];

			UINT8* pESM = pCurBuf + 12 + u32PSILength;
			UINT32 u32Tmp = 0;
			while (u32Tmp < u32ESMLength)
			{
				UINT32 u32StreamID = pESM[1 + u32Tmp];
				u32StreamID &= 0x000000FF;
				u32StreamID |= 0x00000100;
				if (u32StreamID == PS_PES_AUDIO)										//查找音频类型
				{
					m_psAudioType = pESM[0 + u32Tmp];
				}
				else if (u32StreamID == PS_PES_VIDEO)									//视频
				{
					m_psStreamType = pESM[0 + u32Tmp];

					m_psStreamType &= 0x000000FF;
					switch (m_psStreamType)
					{
					case _Stream_MPEG4:
					{
						LOG(DEBUG) << "Stream_MPEG4";
						break;
					}
					case _Stream_H265:
					{
						LOG(DEBUG) << "Stream_H265";
						break;
					}
					case _Stream_H264:
					{
						LOG(DEBUG) << "Stream_H264";
						break;
					}
					case _Stream_SVAC_Video:
					{
						//LOG(DEBUG) << "Stream_SVAC_Video";
						break;
					}
					default:

						break;
					}
				}

				UINT32 u32ESILength = 0;
				u32ESILength = u32ESILength | pESM[2 + u32Tmp];
				u32ESILength = (u32ESILength << 8) | pESM[3 + u32Tmp];
				if (u32ESILength == 0)
				{
					break;
				}
				u32Tmp += (2 + 2 + u32ESILength);

			}

			pCurBuf += psmlength + 4 + 2;
			continue;
		}
		else if (*pcode == 0xE0010000)													//PS视频数据
		{
			int pes_packet_length = *(pCurBuf + 4) * 256 + *(pCurBuf + 5);
			int pes_header_data_length = *(pCurBuf + 8);
			int payload_length = pes_packet_length - 3 - pes_header_data_length;
			pCurBuf += 9 + pes_header_data_length;

			if (payload_length < PSBUFLEN - m_PsBufLen)									//视频数据拼包
			{
				memcpy(m_psbuf + m_PsBufLen, pCurBuf, payload_length);
				m_PsBufLen += payload_length;
			}

			pCurBuf += payload_length;
			continue;
		}
		else if (*pcode == 0xC0010000)													//PS音频数据解析
		{
			int pes_packet_length = *(pCurBuf + 4) * 256 + *(pCurBuf + 5);
			int pes_header_data_length = *(pCurBuf + 8);
			int payload_length = pes_packet_length - 3 - pes_header_data_length;
			pCurBuf += 9 + pes_header_data_length;

			if (m_psAudioType == 155)													//音频解码,只支持(SVAC audio)类型音频，其他类型音频自行处理
			{
				//decode(pCurBuf, payload_length, Audio_frame);
			}
			pCurBuf += payload_length;

			continue;
		}
		else
		{
			break;
		}
	}
	if (m_PsBufLen != 0)
	{

		//这个地方解析出实际帧数据

		Frame frame;
		frame.data = std::vector<uint8_t>(m_psbuf, m_psbuf + m_PsBufLen);
		frame.size = frame.data.size();
		frame.timestamp = 0; // 如果有时间戳信息，可以在这里填充
		_frameNum++;
		if (_onOriginFrame) {
			_onOriginFrame(frame);
		}
		// 如果设置了回调函数，调用回调
		if (_onFrame) {
			//解密34020000001319000111
			//std::string chid = "13102800001310434532";
			//std::string chid = "13100000001310010001";
			//chid = "46020300001321901221";
			if (!_decryptChannel) {
				_decryptChannel = SDKManager::Instance().createChannel(_streamId, ChannelType::DecryptChannel);
				if (!_decryptChannel) return;
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

	m_PsBufLen = 0;

	return;
}


