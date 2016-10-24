#ifndef _CHRTSP_H_
#define _CHRTSP_H_

#ifdef _WIN32_

#ifdef RTSP_DLL
#define RTSP_IN_EXPORT _declspec(dllexport)
#else
#define RTSP_IN_EXPORT _declspec(dllimport)
#endif//RTSP_DLL

#else //_WIN32_
#define RTSP_IN_EXPORT 
#endif//_WIN32_

#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus

/*RTSP 句柄*/
typedef void *CH_RTSP_HANDLE;

/**
* PCM数据结构体
* @sData
* PCM数据指针
* @nDataLen
* PCM长度
* @nSample
* PCM数据采样率
* @nChannels
* PCM数据声道数
* @nTimestampSecond
* PCM数据时间戳秒的部分
* @nTimestampUSecond
* PCM数据时间戳毫秒的部分
* @bIsEnd
* 播放是否结束：0未结束；1结束
* @hRTSP
* RTSP句柄
* @pContext
* 回调上下文
*/
typedef struct _CH_RTSP_PCM_DATA_
{
	unsigned char       *sData;
	unsigned int        nDataLen;
	unsigned int        nSample;
	unsigned int        nChannels;
	unsigned int        nTimestampSecond;
	unsigned int        nTimestampUSecond;
	int                 bIsEnd;

    CH_RTSP_HANDLE      hRTSP;
	void                *pContext;
}CH_RTSP_PCM_DATA;

/*定义PCM数据回调函数*/
typedef int (*CH_RTSP_AudioCallBackFunc)(CH_RTSP_PCM_DATA rdcbd);

/**
* RTSP状态结构体
* @nStatusType
* 0 RTSP连接成功
* 1 RTSP连接失败
* 2 RTSP连接不到音频数据
* 3 RTSP开始重连
* 4 RTSP重连成功
* 5 RTSP重连失败
* @hRTSP
* RTSP句柄
* @pContext
* 回调上下文
*/
typedef struct _CH_RTSP_STATUS_
{
	unsigned int        nStatusType;

	CH_RTSP_HANDLE      hRTSPClient;
	void                *pContext;
}CH_RTSP_STATUS;

/*定义RTSP状态回调函数*/
typedef int (*CH_RTSP_StatusCallBackFunc)(CH_RTSP_STATUS scbd);

/*将PCM数据回调函数传入到RTSP模块中*/
RTSP_IN_EXPORT void CH_RTSP_SetAudioCallBackFunc(CH_RTSP_HANDLE hRTSP, CH_RTSP_AudioCallBackFunc callBack, void* pContext);

/*将状态回调函数传入到RTSP模块中*/
RTSP_IN_EXPORT void CH_RTSP_SetStatusCallBackFunc(CH_RTSP_HANDLE hRTSP, CH_RTSP_StatusCallBackFunc callBack, void* pContext);


/**
* RTSP模块初始化函数
* RTSP只需要调用一次
* @sLogPath
* 日志路径
*/
RTSP_IN_EXPORT int CH_RTSP_Init(const char *sLogPath);

/**
* RTSP模块反初始化函数
* RTSP只需要调用一次
*/
RTSP_IN_EXPORT int CH_RTSP_UnInit();

/**
* RTSP开始连接RTSP Server函数
* @sUrl
* RTSP URL
* @connectMode
* RTSP连接模式
* 0 UDP
* 1 TCP
* @nPcmOutSample
* PCM输出采样率：8000,16000,32000,44100，48000
* @nPlaySecond
* 点播时，文件播放的位置，以秒为单位
* @nReconnectNum
* 重连的次数，默认为2；
* @nReconnectInterval
* 重连的间隔，以秒为单位默认为10秒
* @sCopyPCMFile
* PCM数据备份文件名，为绝对路径，默认为NULL，不备份
@ CH_RTSP_HANDLE
* 返回值
* 非0 RTSP连接成功
* 0   RTSP连接失败
*/
RTSP_IN_EXPORT CH_RTSP_HANDLE CH_RTSP_Start(const char *sUrl,int connectMode, 
											int nPcmOutSample, int nPlaySecond = 0,
											int nReconnectNum = 2, int nReconnectInterval = 10,
											char *sCopyPCMFile = 0);

/**
* RTSP断开连接RTSP Server函数
* @hRTSP
* RTSP 句柄
@ int
* 返回值
* 0 停止成功
* -1   停止失败
*/
RTSP_IN_EXPORT int CH_RTSP_Stop(CH_RTSP_HANDLE hRTSP);

#ifdef __cplusplus
}
#endif//__cplusplus

#endif //_CHRTSP_H_