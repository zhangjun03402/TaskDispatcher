#pragma once

#include <queue>
#include "ChRTSP.h"
#include "CommonFunc.h"
//#include "zhelpers.h"
#include "libczmq\include\czmq.h"

extern int rtsp_status[Max_Threads];
extern int rtsp_asr_status[Max_Threads];

	class CWaveHeader
	{
	public:
		CWaveHeader(void);
		~CWaveHeader(void);
		unsigned int   dwRIFF;
		unsigned int   dwRIFFLen;
		unsigned int   dwWAVE;
		unsigned int   dwfmt;
		unsigned int   dwfmtLen;
		CHWAVEFORMATEX WaveFormat;
		unsigned int   dwdata;
		unsigned int   dwdataLen;
	};

	int RTSP_AudioCallBackFunc(CH_RTSP_PCM_DATA rdcbd);
	int ASR_Pull_RTSP_StreamCallBackFunc(CH_RTSP_PCM_DATA rdcbd);
	//CWaveHeader waveHeader[URL_LEN];
	//FILE *m_fWav;
	//CH_RTSP_HANDLE hCheckClient;

	class GetRtspUrlData
	{
	public:
		GetRtspUrlData();
		~GetRtspUrlData();

		string GetRtspUrl();
		int SetRtspUrl(char *rtspUrl);

		/*************************************************
		* Function: asr_voice_quality_detection
		* Description: 从RtspServer上面获得语音数据
		* Calls: // 被本函数调用的函数清单
		* Called By: // 调用本函数的函数清单
		* Table Accessed: // 被访问的表（此项仅对于牵扯到数据库操作的程序）
		* Table Updated: // 被修改的表（此项仅对于牵扯到数据库操作的程序）
		* Input: //CH_RTSP_HANDLE hCheckClient 输入参数说明，包括每个参数的作
		*		 // 用、取值说明及参数间关系。
		* Output: // 对输出参数的说明。
		* Return: // 函数返回值的说明
		* Others: // 其它说明
		*************************************************/
		bool asr_voice_quality_detection(zframe_t *id,VOICE_CHECK_REQ* req, void *sock,int thread_no );
		bool asr_speech_recognition( zframe_t *id, AUDIO_DETECTOR_REQ* req, void *sock,int thread_no );
		bool UserCancelStream(TASK_CONTROL_INTERFACE &req);
		void clear_msg_list(int thread_no, E_BIZ_NAME biz_name);
		void reset_init_time(int thread_no, E_BIZ_NAME biz_name);
		int  asr_reconn_rtsp(int thread_no,int reconn_count,int reconn_interval, E_BIZ_NAME biz_name);
		E_TASK_CONTROL  check_task_ctrl(char *task_id);
		bool ASRService(zframe_t *id, void *sock, AUDIO_DETECTOR_REQ *req, AUDIO_DETECTOR_RSP *rsp, int cur_time, int *seq_no, char* arr, int *cur_step, int *arr_index, bool isSpeech, int thread_no);
		bool GetTextFromASR(const char *addr, char *waveStream, int waveLen, char *buf, int &bufLen, int thread_no);

	private:
		string m_strRtspUrl;
		//std::queue<st_msg_queue> m_qCheckMsg;
		int  m_seq_no;
		//UCHAR *m_streamBuf;
		//UINT  m_timestamp;
		//UINT  m_streamLen;
		//UINT  m_clientEnd;
	};
	//bool ASRService(zframe_t *id, void *sock, AUDIO_DETECTOR_REQ *req, AUDIO_DETECTOR_RSP *rsp, int cur_time, int *seq_no, char* arr, int *cur_step, int *arr_index, bool isSpeech,int thread_no);
	//void GetTextFromASR(char *addr, char *waveStream, int waveLen, char *buf, int &bufLen,int thread_no);
