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

/*RTSP ���*/
typedef void *CH_RTSP_HANDLE;

/**
* PCM���ݽṹ��
* @sData
* PCM����ָ��
* @nDataLen
* PCM����
* @nSample
* PCM���ݲ�����
* @nChannels
* PCM����������
* @nTimestampSecond
* PCM����ʱ�����Ĳ���
* @nTimestampUSecond
* PCM����ʱ�������Ĳ���
* @bIsEnd
* �����Ƿ������0δ������1����
* @hRTSP
* RTSP���
* @pContext
* �ص�������
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

/*����PCM���ݻص�����*/
typedef int (*CH_RTSP_AudioCallBackFunc)(CH_RTSP_PCM_DATA rdcbd);

/**
* RTSP״̬�ṹ��
* @nStatusType
* 0 RTSP���ӳɹ�
* 1 RTSP����ʧ��
* 2 RTSP���Ӳ�����Ƶ����
* 3 RTSP��ʼ����
* 4 RTSP�����ɹ�
* 5 RTSP����ʧ��
* @hRTSP
* RTSP���
* @pContext
* �ص�������
*/
typedef struct _CH_RTSP_STATUS_
{
	unsigned int        nStatusType;

	CH_RTSP_HANDLE      hRTSPClient;
	void                *pContext;
}CH_RTSP_STATUS;

/*����RTSP״̬�ص�����*/
typedef int (*CH_RTSP_StatusCallBackFunc)(CH_RTSP_STATUS scbd);

/*��PCM���ݻص��������뵽RTSPģ����*/
RTSP_IN_EXPORT void CH_RTSP_SetAudioCallBackFunc(CH_RTSP_HANDLE hRTSP, CH_RTSP_AudioCallBackFunc callBack, void* pContext);

/*��״̬�ص��������뵽RTSPģ����*/
RTSP_IN_EXPORT void CH_RTSP_SetStatusCallBackFunc(CH_RTSP_HANDLE hRTSP, CH_RTSP_StatusCallBackFunc callBack, void* pContext);


/**
* RTSPģ���ʼ������
* RTSPֻ��Ҫ����һ��
* @sLogPath
* ��־·��
*/
RTSP_IN_EXPORT int CH_RTSP_Init(const char *sLogPath);

/**
* RTSPģ�鷴��ʼ������
* RTSPֻ��Ҫ����һ��
*/
RTSP_IN_EXPORT int CH_RTSP_UnInit();

/**
* RTSP��ʼ����RTSP Server����
* @sUrl
* RTSP URL
* @connectMode
* RTSP����ģʽ
* 0 UDP
* 1 TCP
* @nPcmOutSample
* PCM��������ʣ�8000,16000,32000,44100��48000
* @nPlaySecond
* �㲥ʱ���ļ����ŵ�λ�ã�����Ϊ��λ
* @nReconnectNum
* �����Ĵ�����Ĭ��Ϊ2��
* @nReconnectInterval
* �����ļ��������Ϊ��λĬ��Ϊ10��
* @sCopyPCMFile
* PCM���ݱ����ļ�����Ϊ����·����Ĭ��ΪNULL��������
@ CH_RTSP_HANDLE
* ����ֵ
* ��0 RTSP���ӳɹ�
* 0   RTSP����ʧ��
*/
RTSP_IN_EXPORT CH_RTSP_HANDLE CH_RTSP_Start(const char *sUrl,int connectMode, 
											int nPcmOutSample, int nPlaySecond = 0,
											int nReconnectNum = 2, int nReconnectInterval = 10,
											char *sCopyPCMFile = 0);

/**
* RTSP�Ͽ�����RTSP Server����
* @hRTSP
* RTSP ���
@ int
* ����ֵ
* 0 ֹͣ�ɹ�
* -1   ֹͣʧ��
*/
RTSP_IN_EXPORT int CH_RTSP_Stop(CH_RTSP_HANDLE hRTSP);

#ifdef __cplusplus
}
#endif//__cplusplus

#endif //_CHRTSP_H_