#ifndef DEFS_H
#define DEFS_H

#include<vector>
#include <cstdint>


enum
{
	I_frame = 1,
	B_frame = 2,
	P_frame = 3,
	Audio_frame = 4,
};

struct Frame {

	bool keyFrame;
    bool hasSPS = false;
	std::vector<uint8_t> data;
	size_t size;
	uint64_t timestamp;
    //文件中的偏移
    size_t offset;
    //帧序号
    size_t frameNum;
	int width;
	int height;

};







#endif
