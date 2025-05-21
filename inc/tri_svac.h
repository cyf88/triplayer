#ifndef __trisvacapi__
#define __trisvacapi__

#include <Windows.h>
#define TRI_RET unsigned long __stdcall

#include "ZxSvacDecLib.h"

//#ifdef  __cplusplus
//extern "C" {
//#endif


	typedef struct VideoDecodeCtx {
		HANDLE videoDecodeHandle;
		SVAC_PREFETCH_PARAM svac_param;
		int m_BytePerSample;
		unsigned char* m_buf = NULL;
		unsigned char* m_svc_buf = NULL;
		unsigned char* m_pBufDecode = NULL;
		bool m_FindIFrame;

	} Vd_ctx;

	TRI_RET TRI_SVAC_INIT();
	TRI_RET TRI_SVAC_DECODE_INIT(VideoDecodeCtx* vdCtx, unsigned char* input, ULONG inputSize, unsigned int frameType, int threadNum);
	TRI_RET TRI_SVAC_DECODE(VideoDecodeCtx* vdCtx, unsigned char* input, ULONG inputSize, unsigned char* output, ULONG* outputSize, int& width, int& height);
	TRI_RET TRI_SVAC_DECODE_UNINIT(VideoDecodeCtx* vdCtx);
	TRI_RET TRI_SVAC_DESTORY();
//#ifdef  __cplusplus
//}
//#endif  /* #ifdef  __cplusplus */

#endif
