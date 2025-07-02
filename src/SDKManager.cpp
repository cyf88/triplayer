#include "SDKManager.h"
#include "tri_core_api.h"
#include "easylogging++.h"
#include "config.h"


//#define FM_SKF

Channel::~Channel() {
	if (_channelId >= 0) {
		TRI_SUT_Close(_channelId);
	}
}

void DecryptionChannel::doWork(Frame& input, Frame& output) const {
	if (_channelId < 0) {
		LOG(ERROR) << "no channel found";
		output = input;
		return;
	}
	IMG_FRAME_UNIT in;
	in.img_buf = input.data.data();
	in.imgsz = input.size;
	IMG_FRAME_UNIT* decryptFrame = NULL;
	IMG_FRAME_UNIT* out = NULL;
	int ret = TRI_SUT_VideoDataSecDecodeExt((ULONG*)(&_channelId), &in, &out, true);
	if (ret != 0) {
		LOG(ERROR) << "TRI_SUT_VideoDataSecDecodeExt fail";
		output = input;
		TRI_SUT_Free(&out);
		return;
	}

	output.data = std::vector<uint8_t>(out->img_buf, out->img_buf + out->imgsz);
	output.size = out->imgsz;
	output.keyFrame = (out->type == 'I') ? true : false;
    output.frameNum = input.frameNum;
	TRI_SUT_Free(&out);
	return;
}

void ExportChannel::doWork(Frame& input, Frame& output) const {
	if (_channelId < 0) {
		LOG(ERROR) << "no channel found";
		output = input;
		return;
	}
	IMG_FRAME_UNIT in;
	in.img_buf = input.data.data();
	in.imgsz = input.size;
	IMG_FRAME_UNIT* out = NULL;
	int ret = TRI_SUT_VideoDataSecExportExt((ULONG*)(&_channelId), &in, &out);
	if (ret != 0) {
		LOG(ERROR) << "TRI_SUT_VideoDataSecDecodeExt fail";
		output = input;
		TRI_SUT_Free(&out);
		return;
	}

	output.data = std::vector<uint8_t>(out->img_buf, out->img_buf + out->imgsz);
	output.size = out->imgsz;
	output.keyFrame = (out->type == 'I') ? true : false;
	TRI_SUT_Free(&out);
	return;
}

void DownloadChannel::doWork(Frame& input, Frame& output) const {
	if (_channelId < 0) {
		LOG(ERROR) << "no channel found";
		output = input;
		return;
	}
	IMG_FRAME_UNIT in;
	in.img_buf = input.data.data();
	in.imgsz = input.size;
	IMG_FRAME_UNIT* out = NULL;
	int ret = TRI_SUT_VideoDataSecDownload(_channelId, &in, &out);
	if (ret != 0) {
		LOG(ERROR) << "TRI_SUT_VideoDataSecDecodeExt fail";
		output = input;
		TRI_SUT_Free(&out);
		return;
	}

	output.data = std::vector<uint8_t>(out->img_buf, out->img_buf + out->imgsz);
	output.size = out->imgsz;
	output.keyFrame = (out->type == 'I') ? true : false;
	TRI_SUT_Free(&out);
	return;
}

DownloadChannel::~DownloadChannel() {
	if (_channelId >= 0) {
		TRI_SUT_CleanupForSecDownload(_channelId);
		TRI_SUT_Close(_channelId);
	}
}

SDKManager::SDKManager() {


	std::string ukeyPin = ConfigReader::Instance().getValue("Player", "ukey_pin");
	LOG(INFO) << "ukeyPin: " << ukeyPin;
	if (!TRI_SUT_Init((char*)ukeyPin.c_str())) {
		LOG(INFO) << "TRI_SUT_Init fail with pin:" << ukeyPin;
	}

	std::string serverId = ConfigReader::Instance().getValue("SIP_CONFIG", "server_id");
	std::string serverIp = ConfigReader::Instance().getValue("SIP_CONFIG", "server_ip");
	std::string serverPort = ConfigReader::Instance().getValue("SIP_CONFIG", "server_port");
	LOG(INFO) << "SIP_CONFIG  serverId: " << serverId <<
		" serverIp: " << serverIp <<
		" serverPort: " << serverPort;
	TRI_SUT_SetConfigParameter(TRI_PARAM_GB_Code, (char*)serverId.c_str());
	TRI_SUT_SetConfigParameter(TRI_PARAM_SERVER_IP, (char*)serverIp.c_str());
	TRI_SUT_SetConfigParameter(TRI_PARAM_SERVER_PORT, (char*)serverPort.c_str());


	char* userid = NULL;
	unsigned long useridLen = 0;
	char* token = NULL;
	unsigned long tokenLen = 0;
    TRI_RET ret = TRI_SUT_Login(&userid, &useridLen, &token, &tokenLen);
    if (ret != 0) {
        LOG(ERROR) << "TRI_SUT_Login Fail";
    }

	LOG(INFO) << "SDKManager Create";
	
}

SDKManager::~SDKManager() {
	TRI_SUT_Cleanup();
	LOG(INFO) << "SDKManager Destroy";
}

Channel::Ptr SDKManager::createChannel(std::string deviceId, ChannelType type) {
	if (type == ChannelType::DecryptChannel) {
		ULONG decodeHandle;
		TRI_RET ret = TRI_SUT_GetStreamSecDecode(&decodeHandle);
		if (ret != 0) {
			LOG(ERROR) << "create DecryptChannel ret: " << ret;
			return nullptr;
		}
		//debug  此处为了测试拿到实时点播的vkek
        // ret = TRI_SUT_GetRealStreamSecDecode(&decodeHandle, (char*)deviceId.c_str());
        // if (ret != 0) {
        //     TRI_SUT_Close(decodeHandle);
        //     LOG(ERROR) << "create DecryptChannel ret: " << ret;
        //     //return nullptr;
        // }

        // ret = TRI_SUT_GetPlayBackSecDecode(&decodeHandle, (char*)deviceId.c_str(), NULL, NULL);
        // if (ret != 0) {
        //     TRI_SUT_Close(decodeHandle);
        //     LOG(ERROR) << "create DecryptChannel ret: " << ret;
        //     //return nullptr;
        // }
		auto channel = std::make_shared<DecryptionChannel>(decodeHandle, deviceId);
		return channel;
	}
	else if (type == ChannelType::ExportChannel) {
		ULONG handle;
		TRI_RET ret = TRI_SUT_GetStreamSecExport((char*)deviceId.c_str(), &handle);
		if (ret != 0) {
			LOG(ERROR) << "create ExportChannel ret: " << ret;
			return nullptr;
		}
        TRI_SUT_SetExportRightInfo(&handle, 5, (char*)"2025-02-20");
		auto channel = std::make_shared<ExportChannel>(handle, deviceId);
		return channel;
	}
	else if (type == ChannelType::DownloadChannel) {
		ULONG decodeHandle;
		TRI_RET ret = TRI_SUT_GetStreamSecDecode(&decodeHandle);
		if (ret != 0) {
			LOG(ERROR) << "create DecryptChannel ret: " << ret;
			return nullptr;
		}
		ret = TRI_SUT_GetPlayBackSecDecode(&decodeHandle, (char*)deviceId.c_str(), NULL, NULL);
		if (ret != 0) {
			TRI_SUT_Close(decodeHandle);
			LOG(ERROR) << "create DecryptChannel ret: " << ret;
			return nullptr;
		}
		ret = TRI_SUT_StartupForSecDownload(decodeHandle);
		if (ret != 0) {
			TRI_SUT_Close(decodeHandle);
			LOG(ERROR) << "create DecryptChannel ret: " << ret;
			return nullptr;
		}
		auto channel = std::make_shared<DownloadChannel>(decodeHandle, deviceId);
		return channel;
	}
    return nullptr;
}

void SDKManager::releaseChannel(Channel::Ptr &channel) {
	TRI_SUT_Close(channel->getChannelId());
}


int SDKManager::queryPlayRightInfo(unsigned char* buffer, int bufferSize, unsigned int& spandays, unsigned int& spanCounts) {
    TRI_RET ret = TRI_SUT_QueryVideoPlayRightInfo((char*)"test.ps", buffer, bufferSize, &spandays, &spanCounts);
	if (ret != 0) {
		LOG(ERROR) << "TRI_SUT_QueryVideoPlayRightInfo fail";
		return ret;
	}
	return 0;
}

int SDKManager::setPlayRightInfo(unsigned char* buffer, int bufferSize, char* deadline, unsigned int playCounts) {
    TRI_RET ret = TRI_SUT_SetVideoPlayRightInfo((char*)"test.ps", buffer, bufferSize, deadline, playCounts);
    if (ret != 0) {
        LOG(ERROR) << "TRI_SUT_SetVideoPlayRightInfo fail deadline: " << deadline << " playCounts: " << playCounts;
        return ret;
    }
    return 0;
}


