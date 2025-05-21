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
	std::vector<uint8_t> data;
	size_t size;
	uint64_t timestamp;
	int width;
	int height;

};







#endif