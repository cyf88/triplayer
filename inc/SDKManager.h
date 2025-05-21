#ifndef SDKMANAGER_H
#define SDKMANAGER_H

#include "defs.h"
#include <string>
#include <memory>

enum class ChannelType {
	DecryptChannel,
	EncryptChannel,
	ExportChannel,
	DownloadChannel
};

class Channel {
public:
	using Ptr = std::shared_ptr<Channel>;

	virtual void doWork(Frame& input, Frame& output) const = 0;

	int getChannelId() {
		return _channelId;
	}

	Channel(int channelId, std::string deviceId) {
		_channelId = channelId;
		_deviceId = deviceId;
	}
	~Channel();

protected:
	int _channelId = -1;
	std::string _deviceId;
	
};

class DecryptionChannel : public Channel {

public:
	//重用父类构造函数
	using Channel::Channel;
	using Ptr = std::shared_ptr<DecryptionChannel>;	
	void doWork(Frame& input, Frame& output) const override;	
};


class ExportChannel : public Channel {

public:
	using Channel::Channel;
	using Ptr = std::shared_ptr<ExportChannel>;
	void doWork(Frame& input, Frame& output) const override;
};

//使用类似一所接口进行文件导出
class DownloadChannel : public Channel {
public:
	using Channel::Channel;
	~DownloadChannel();
	using Ptr = std::shared_ptr<DownloadChannel>;
	void doWork(Frame& input, Frame& output) const override;
};

class SDKManager {

public:

	static SDKManager& Instance() {
		static SDKManager instance;  // 静态局部变量，保证实例唯一
		return instance;
	}

	Channel::Ptr createChannel(std::string deviceId, ChannelType type);

	void releaseChannel(Channel::Ptr &channel);
		
private:	
	SDKManager();
	~SDKManager();

};











#endif