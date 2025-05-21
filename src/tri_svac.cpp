#include "tri_svac.h"
#include "easylogging++.h"
#include "defs.h"
INITIALIZE_EASYLOGGINGPP

#define PSBUFLEN (5 * 1024 * 1024)


    static int GetBasicYSize(int nYSize, int nRadio) {
    int nBasicYSize = nYSize;
    switch (nRadio) {
    case 0: {
        nBasicYSize = nYSize * 9 / 16;
        break;
    }
    case 1: {
        nBasicYSize = nYSize * 4 / 16;
        break;
    }
    case 2: {
        nBasicYSize = nYSize * 1 / 16;
        break;
    }
    case 3: {
        nBasicYSize = nYSize * 1 / 36;
        break;
    }
    case 4: {
        nBasicYSize = nYSize * 1 / 64;
    }
    default:
        break;
    }

    return nBasicYSize;
}

static int ModifyVerValue(int _iVersion, int _iWidth, int _iHeight)
{
    int iVersion = _iVersion;
    iVersion |= (_iWidth << 2);
    iVersion |= (_iHeight << 16);
    return iVersion;
}

static int ModifyThreadValue(int _iThreadNum, SVAC_PREFETCH_PARAM _stuParam)
{
    int iThreadNum = _iThreadNum;
    int iSSVC = _stuParam.spatial_svc_flag == 0 ? 1 : 0;
    iThreadNum |= (iSSVC << 4);
    int iSsvcRatio = 0;
    if (_stuParam.spatial_svc_ratio > 0)
    {
        iSsvcRatio = _stuParam.spatial_svc_ratio - 1;
    }
    if (_stuParam.spatial_svc_ratio == 0)
    {
        iSsvcRatio = 4;
    }
    iThreadNum |= (iSsvcRatio << 5);
    int iBit = _stuParam.bit_depth_luma == 10 ? 0 : 1;
    iThreadNum |= (iBit << 8);
    int iConsult = _stuParam.reserved[0] == 0 ? 5 : _stuParam.reserved[0];
    iThreadNum |= (iConsult << 10);
    int iExtended_sb_size = _stuParam.reserved[1] == 0 ? 1 : 0;
    iThreadNum |= (iExtended_sb_size << 13);
    int iSAO = _stuParam.reserved[2] == 0 ? 1 : 0;
    iThreadNum |= (iSAO << 14);
    return iThreadNum;
}

TRI_RET TRI_SVAC_INIT() {
    int ret = ZXSVACDec_Init();
    if (ret < 0) {
        LOG(DEBUG) << "ZXSVACDec_Init failed";
        return -ret;
    }
    //SVAC_IS_INITED = TRUE;
    return 0;
}

TRI_RET TRI_SVAC_DECODE_INIT(VideoDecodeCtx* vdCtx,unsigned char* input, ULONG inputSize,unsigned int frameType, int threadNum) {
    int ret = -1;

    //首先进行探针，获取必要的码流信息
    if (frameType == I_frame) {
        //只有I帧数据，需要生成PARAM
        if (vdCtx->videoDecodeHandle == HANDLE(-1)) {
            //未初始化通道
            ret = ZXSVACDec_GetHandle(vdCtx->videoDecodeHandle);
            if (ret < 0) {
                LOG(DEBUG) << "ZXSVACDec_GetHandle ret=" << ret;
                return - ret;
            }
        }
        SVAC_PREFETCH_PARAM param;
        memset(&param, 0, sizeof(SVAC_PREFETCH_PARAM));
        ret = ZXSVACDec_PrefetchParam(NULL, input, inputSize, &param);
        if (ret != 0) {
            LOG(DEBUG) << "ZXSVACDec_PrefetchParam failed ret = 2";
            return 2;
        }
        if (memcmp(&(vdCtx->svac_param), &param, sizeof(SVAC_PREFETCH_PARAM)) != 0) {
            memcpy(&vdCtx->svac_param, &param, sizeof(SVAC_PREFETCH_PARAM));
            vdCtx->m_BytePerSample = vdCtx->svac_param.bit_depth_luma == 8 ? 1 : 2;
            SAFE_DELETE_ARRAY(vdCtx->m_buf);
            SAFE_DELETE_ARRAY(vdCtx->m_svc_buf);
            int nYSize = vdCtx->svac_param.width * vdCtx->svac_param.height;
            if (vdCtx->svac_param.spatial_svc_flag != 0)
            {
                int nBasicYSize = GetBasicYSize(nYSize, vdCtx->svac_param.spatial_svc_ratio);				//根据增强层空间大小和所占比，计算基本层的所占空间大小。
                vdCtx->m_buf = new unsigned char[vdCtx->m_BytePerSample * nBasicYSize * 3 / 2 + 16];
                vdCtx->m_svc_buf = new unsigned char[vdCtx->m_BytePerSample * nYSize * 3 / 2 + 16];
            }
            else
            {
                vdCtx->m_buf = new unsigned char[vdCtx->m_BytePerSample * nYSize * 3 / 2 + 16];
            }
            if (vdCtx->m_buf == NULL)
            {
                return 3;
            }
            int res = 0;
            if (vdCtx->videoDecodeHandle != HANDLE(-1))
            {
                int ver = vdCtx->svac_param.svac_version;
                if (vdCtx->svac_param.width > 1920 || vdCtx->svac_param.height > 1088)
                {
                    ver = ModifyVerValue(vdCtx->svac_param.svac_version, vdCtx->svac_param.width, vdCtx->svac_param.height);
                }
                int iThreadnum = ModifyThreadValue(threadNum, param);
                //ZXSVACDec_Open返回值非0为成功，非0的数字是授权剩余天数。当正式授权过期时，也可以进行解码（m_SvacHandle为有效值），但解码的YUV会有logo
                res = ZXSVACDec_Open(vdCtx->videoDecodeHandle, iThreadnum, -1, ver);
                LOG(DEBUG) << "ZXSVACDec_Open res = " << res;
                if (res >= 0) {
                    vdCtx->m_FindIFrame = true;
                    return 0;
                }
                else {
                    return res;
                }

            }
        }
    }

    return ret;
}
TRI_RET TRI_SVAC_DECODE_UNINIT(VideoDecodeCtx* vdCtx) {

    if (vdCtx == NULL) {
        return 0;
    }
    if (vdCtx->videoDecodeHandle != NULL) {
        ZXSVACDec_Close(vdCtx->videoDecodeHandle);
    }
    if (vdCtx->m_pBufDecode != NULL) {
        free(vdCtx->m_pBufDecode);
    }
    if (vdCtx->m_buf != NULL) {
        delete vdCtx->m_buf;
        vdCtx->m_buf = NULL;
    }
    if (vdCtx->m_svc_buf != NULL) {
        delete vdCtx->m_svc_buf;
        vdCtx->m_svc_buf = NULL;
    }
    vdCtx->m_FindIFrame = false;
    return 0;
}

TRI_RET TRI_SVAC_DECODE(VideoDecodeCtx* vdCtx,unsigned char* input, ULONG inputSize,unsigned char* output,ULONG * outputSize, int& width, int& height) {
    int ret = 0;
    DEC_INPUT_PARAM		dec_in;
    memset(&dec_in, 0, sizeof(DEC_INPUT_PARAM));
    DEC_OUTPUT_PARAM	dec_out;
    memset(&dec_out, 0, sizeof(DEC_OUTPUT_PARAM));
    int offset = 16 - (int)vdCtx->m_buf % 16;
    int svc_offset = 16 - (int)vdCtx->m_svc_buf % 16;

    int nYSize = vdCtx->svac_param.width * vdCtx->svac_param.height;
    int nUVSize = nYSize / 4;
    //LOG(DEBUG) << "TRI_SVAC_DECODE 1";
    if (vdCtx->svac_param.spatial_svc_flag)					//当开启SVC时
    {

        int nBasicYSize = GetBasicYSize(nYSize, vdCtx->svac_param.spatial_svc_ratio);		//转化获取基本层大小
        int nBasicUVSize = GetBasicYSize(nUVSize, vdCtx->svac_param.spatial_svc_ratio);

        unsigned char* dest_buf = vdCtx->m_buf + (offset == 16 ? 0 : offset);
        dec_out.pY = dest_buf;
        dec_out.pU = dest_buf + vdCtx->m_BytePerSample * nBasicYSize;
        dec_out.pV = dest_buf + vdCtx->m_BytePerSample * (nBasicYSize + nBasicUVSize);

        unsigned char* m_pSvcBuf = vdCtx->m_svc_buf + (svc_offset == 16 ? 0 : svc_offset);
        dec_out.pY_SVC = m_pSvcBuf;
        dec_out.pU_SVC = m_pSvcBuf + vdCtx->m_BytePerSample * nYSize;
        dec_out.pV_SVC = m_pSvcBuf + vdCtx->m_BytePerSample * (nYSize + nUVSize);
    }
    else
    {
        unsigned char* dest_buf = vdCtx->m_buf + (offset == 16 ? 0 : offset);
        dec_out.pY = dest_buf;
        dec_out.pU = dest_buf + vdCtx->m_BytePerSample * nYSize;
        dec_out.pV = dest_buf + vdCtx->m_BytePerSample * (nYSize + nUVSize);
    }
    //LOG(DEBUG) << "TRI_SVAC_DECODE 2";
    int tryCnt = 0;
    if (vdCtx->m_pBufDecode == NULL)
    {
        vdCtx->m_pBufDecode = new unsigned char[PSBUFLEN];
        if (vdCtx->m_pBufDecode == NULL)
        {
            *outputSize = 0;
            return 1;
        }
    }
    while (tryCnt < 2) {
        dec_in.pBitstream = input;
        dec_in.nLen = inputSize;
        dec_in.chroma_format_idc = 1;
        dec_in.bSvcdec = 1;
        dec_in.bExtdecOnly = 0;
        dec_in.check_authentication_flag = 0;
        dec_out.nWidth = vdCtx->svac_param.width;
        dec_out.nHeight = vdCtx->svac_param.height;
        //LOG(DEBUG) << "ZXSVACDec_Decode start";
        ret = ZXSVACDec_Decode(vdCtx->videoDecodeHandle, &dec_in, &dec_out, NULL);
        //LOG(DEBUG) << "ZXSVACDec_Decode end";
        if (ret > 0) {
            break;
        }
        tryCnt++;
    }

    if (ret == -1) {
        *outputSize = 0;
        LOG(DEBUG) << "decode error ret=" << ret;
        return 3;
    }
    if (ret >= 0) {
        if ((dec_out.nIsEffect & 0x1) == 0) {
            LOG(DEBUG) << "dec_out.nIsEffect & 0x1 = 0";
            *outputSize = 0;
            return 2;
        }
    }
    else {
        if (dec_out.nWidth <= 0) {
            *outputSize = 0;
            LOG(DEBUG) << "decode error ret=" << ret;
            return 3;
        }

    }
    //*frameWidth = dec_out.nWidth;
    //*frameHeight = dec_out.nHeight;
    ULONG pixelCount = dec_out.nWidth * dec_out.nHeight;
    //LOG(DEBUG) << "pixelCount:" << pixelCount;
    memcpy(output, dec_out.pY, pixelCount);
    memcpy(output + pixelCount, dec_out.pU, pixelCount/4);
    memcpy(output + pixelCount + pixelCount / 4, dec_out.pV, pixelCount / 4);
    //LOG(DEBUG) << "ZXSVACDec_Decode cpy end";
    *outputSize = pixelCount + pixelCount / 2;
    width = dec_out.nWidth;
    height = dec_out.nHeight;
    return 0;
}

TRI_RET TRI_SVAC_DESTORY() {

    LOG(DEBUG) << "ZXSVACDec_UnInit";
    int ret = ZXSVACDec_UnInit();
    if (ret != 0) {
        LOG(DEBUG) << "ZXSVACDec_UnInit error";
    }
    return ret;
}
