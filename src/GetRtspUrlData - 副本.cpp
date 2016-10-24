#include "..\inc\GetRtspUrlData.h"
#include "..\inc\CommonFunc.h"
#include "..\inc\AudioStateChecker.h"
#include "..\inc\TaskControl.h"
#include "..\inc\TaskDispatch.h"
#include "..\inc\VoiceActivityDetector.h"
#include "..\inc\zhelpers.h"
#include "..\inc\StatusCodeHead.h"
#include "..\inc\PublicHead.h"

#include "glog\logging.h"

#include "code.h"

#include <fstream>

#include <thread>
#include <queue>
#include <map>
#include <string>
#include <iostream>
using namespace std;
//using namespace Lingban::Common;

#define _WIN32_
#ifdef _WIN32_
#include <windows.h>
void local_sleep(int seconds)
{
	Sleep(seconds*1000);
}
#else
#include <unistd.h>
void local_sleep(int seconds)
{
	sleep(seconds);
}
#endif

extern CRITICAL_SECTION g_csCheck[Max_Threads];  //主意质量检测临界区
extern CRITICAL_SECTION g_callback_check;  //语音质量全局临界区，主要是因为注册拉流的参数是全局的

extern queue< st_msg_queue> g_msg_queue_check[Max_Threads];
extern queue< st_msg_queue> g_msg_queue_asr[Max_Threads];

extern CRITICAL_SECTION g_csAsr[Max_Threads];
extern CRITICAL_SECTION g_callback_asr;

extern map< string, st_task_ctrl> g_map_ctrl;
extern CRITICAL_SECTION g_cs_map_ctrl;  //主意质量检测临界区

extern char g_asr_serv[64];
extern char g_appid[32];
extern char g_key[64];

extern HANDLE  g_hAsrSemaphore[Max_Threads];

CH_RTSP_HANDLE hClient[Max_Threads];
CH_RTSP_HANDLE hAsrClient[Max_Threads];

FILE* fp_checkWav[Max_Threads];
FILE* fp_asrWav[Max_Threads];
FILE *fp_recog[Max_Threads];

int rtsp_status[Max_Threads] = { -1 };
int rtsp_asr_status[Max_Threads] = { -1 };

int clientEnd = -1;
unsigned int UInitTimeSecond[Max_Threads] = { 0 };
unsigned int UInitTimeUSecond[Max_Threads] = { 0 };

UINT asr_uinit_time_sec[Max_Threads]  = { 0 };
UINT asr_uinit_time_usec[Max_Threads] = { 0 };

//#define _ASR_TEST_

CWaveHeader::CWaveHeader(void)
	: dwRIFF(1179011410)
	, dwRIFFLen(38)
	, dwWAVE(1163280727)
	, dwfmt(544501094)
	, dwfmtLen(18)
	, dwdata(1635017060)
	, dwdataLen(0)
{
}

CWaveHeader::~CWaveHeader(void)
{
}

GetRtspUrlData::GetRtspUrlData()
{
	::memset(m_aRtspUrl, 0x00, sizeof(m_aRtspUrl));
	m_seq_no = 0;
}

GetRtspUrlData::~GetRtspUrlData()
{
}

int GetRtspUrlData::SetRtspUrl(char * rtspUrl)
{
	::memcpy(m_aRtspUrl, rtspUrl, sizeof(m_aRtspUrl));
	return 0;
}

char* GetRtspUrlData::GetRtspUrl()
{
	return m_aRtspUrl;
}

/* RTSP状态结构体
* @nStatusType
* 0 RTSP连接成功           1 RTSP连接失败
* 2 RTSP连接不到音频数据   3 RTSP重连成功
* 4 RTSP重连失败
* @hRTSP
* RTSP句柄
* @pContext
* 回调上下文
*/
int RTSP_StatusCallBackFunc(CH_RTSP_STATUS rdcbd)
{
	cout << "Enter " << __FUNCTION__ << " process..." << endl;

	int thread_no = 0;
	for (thread_no = 0;thread_no < Max_Threads;thread_no++)
	{
		if ( rdcbd.hRTSPClient== hClient[thread_no])
		{
			break;
		}
	}

	rtsp_status[thread_no] = rdcbd.nStatusType;
	LOG(INFO)<< "Enter "<<__FUNCTION__<<" process...,rtsp_status:"<<rtsp_status;
	return rdcbd.nStatusType;
}

int RTSP_AsrStatusCallBackFunc(CH_RTSP_STATUS rdcbd)
{
	cout << "Enter " << __FUNCTION__ << " process..." << endl;

	int thread_no = 0;
	for (thread_no = 0;thread_no < Max_Threads;thread_no++)
	{
		if (rdcbd.hRTSPClient == hAsrClient[thread_no])
		{
			break;
		}
	}

	rtsp_asr_status[thread_no] = rdcbd.nStatusType;
	cout << "Leave " << __FUNCTION__ << " process...,rtsp status:[" << rtsp_status << "]" << endl;
	return rdcbd.nStatusType;
}


void GetRtspUrlData::UserCancelStream(zframe_t *id, TASK_CONTROL_INTERFACE &req, void *sock, int thread_no)
{
	TASK_CTRL_INTER_RSP rsp;
	memcpy(rsp.task_id, req.task_id, strlen(req.task_id) + 1);
	
	st_task_ctrl st;
	while (true)
	{
		EnterCriticalSection(&g_cs_map_ctrl);
		auto it = g_map_ctrl.find(req.task_id);
		if (it != g_map_ctrl.end())
		{
			st = it->second;
			if ( E_CTRL_SUCC== st.ctrl_type )
			{
				/* */
				memcpy(rsp.result, "success", sizeof("success"));
				send_rsp_2_task_ctrl(id, sock, rsp);
				g_map_ctrl.erase(it);
				LeaveCriticalSection(&g_cs_map_ctrl);
				break;
			}
		}
		LeaveCriticalSection(&g_cs_map_ctrl);
		local_sleep(1);
	}
	cout<<"task cancel success!task_id:" << req.task_id;
	LOG(INFO) << "task control:business cancel success!task_id:" << req.task_id;
	return;
}

/*************************************************
Function:    asr_voice_quality_detection
Description: 噪音检测实际业务处理程序
Input:       zframe_t *id 消息帧头信息
VOICE_CHECK_REQ *req 噪音检测请求结构体
void *sock  创建的套接字
Output:      NULl
Return:      void
Others:      由于请求结构体中有推送间隔，开始时间，处理
时间。之前是在推送间隔的数据保存到vector，推送时
间到达时，把vector里面的数据全部推送出去，经和郭
的沟通，是只发送推送间隔那一刻检测到的值
*************************************************/
void GetRtspUrlData::asr_voice_quality_detection(zframe_t *id, VOICE_CHECK_REQ* req, void* sock, int thread_no)
{
	LOG( INFO )<<"Enter "<<__FUNCTION__<<" process...";

	//char fileName[FILE_NAME_LEN] = { 0 };
	//sprintf(fileName, "./store_wav/check_%s_%d.pcm", req->task_id, thread_no);
	//fp_checkWav[thread_no] = fopen(fileName, "ab+");
	
	AudioStateChecker *mASC = new AudioStateChecker(8000, 512, 1875); // 1875: 30s, 3750: 60s
	if (nullptr == mASC)
	{
		LOG(INFO) << "new audio state check class is null";
		return;
	}

	int seq_no = 0; //序列号
	int i = 0;

	const int bufferSize = 256;
	int sbufferSize = bufferSize >> 1;
	char buffer[bufferSize] = { 0 };
	short* sbuffer = new short[bufferSize >> 1];
	float* fbuffer = new float[bufferSize >> 1];

	reset_init_time(thread_no, E_BIZ_NAME_CHECK);
	clear_msg_list(thread_no, E_BIZ_NAME_CHECK);

	int play_seconds = req->start_time * 1000; /* 传来的数据是毫秒，SDK需要的时间单位是秒 */
	int reconn_num = (req->reconn_num == 0 ? 2 : req->reconn_num);
	int interval = (req->reconn_interval == 0 ? 10 : req->reconn_interval);
	hClient[thread_no] = CH_RTSP_Start( m_aRtspUrl, E_RTSP_CONN_MODE_TCP, 8000, play_seconds,reconn_num,interval,req->file_path);
	//hClient[thread_no] = CH_RTSP_Start( m_aRtspUrl, E_RTSP_CONN_MODE_UDP, 8000 );
	if (0 == hClient[thread_no])
	{
		LOG(INFO)<< "Check start rtsp error!thread_id["<<GetCurrentThreadId()<<"]" ;
	}

	CH_RTSP_SetAudioCallBackFunc(hClient[thread_no], RTSP_AudioCallBackFunc, NULL);
	CH_RTSP_SetStatusCallBackFunc(hClient[thread_no], RTSP_StatusCallBackFunc, NULL);

	int nRead = 0;
	int cnt_len = 0; //统计当时总长度，为了统计时间所用
	int cnt_push = 0;
	int push_count = 0; //用于判断是否需要推送的记录

	char *arr = nullptr;
	int arr_len = 16000;
	int arr_index = 0;
	int cur_step = 0;
	
	int bc = 0;
	int last_id = 0;

	VOICE_CHECK_RSP rsp = { 0 };
	rsp.result.start_time_stamp = 0;
	rsp.result.end_time_stamp = 0;

	LOG( INFO )<<"begin process voice check...";

	int end_type = 0;
	int end_flag = false; //结束标志
	unsigned int cnt_time = (8000/1000) * 16/8  ; /* 每毫秒数据占的字节数 */
	unsigned int start_time = 0;
	unsigned int end_time   = 0;
	int timeout = 0;
	st_task_ctrl st;

	while (!end_flag)
	{
		/* 取表中的数据 */
		if (g_msg_queue_check[thread_no].empty())
		{
			/* rtsp连接状态 */
			if (E_RTSP_STATUS_RECON_SUCC == rtsp_status[thread_no] || E_RTSP_STATUS_CONN_SUCC == rtsp_status[thread_no])
			{
				if (req->start_time > start_time)
				{
					CH_RTSP_Stop(hClient[thread_no]);
					hClient[thread_no] = CH_RTSP_Start(m_aRtspUrl, E_RTSP_CONN_MODE_TCP, 8000, play_seconds, reconn_num, interval, req->file_path);
					CH_RTSP_SetAudioCallBackFunc(hClient[thread_no], RTSP_AudioCallBackFunc, NULL);
					CH_RTSP_SetStatusCallBackFunc(hClient[thread_no], RTSP_StatusCallBackFunc, NULL);
				}
				timeout = 0;
				continue;
			}
			local_sleep(interval);
			if (timeout >= reconn_num)
			{
				LOG(INFO)<< "reconnect rtsp fail!status["<< rtsp_status[thread_no]<<"]";
				end_flag = true;
				VOICE_CHECK_RSP rsp;
				noise_send_2_request(id, sock, rsp, req->task_id, E_MSG_TYPE_BIZ_FAIL);
			}
			timeout++;
			continue;
		}
		timeout = 0;

		EnterCriticalSection(&g_csCheck[thread_no]);
		st_msg_queue msg_queue = g_msg_queue_check[thread_no].front(); //取头元素
		g_msg_queue_check[thread_no].pop(); //删除队头，指针后面释放
		LeaveCriticalSection(&g_csCheck[thread_no]);

		int play_flag = msg_queue.end_flag;
		if (E_PLAY_OVER == play_flag)
		{
			LOG(INFO)<< "the end flag is "<< play_flag;
			break;
		}

		/* 把转换为单通道之后的数据保存下来*/
		//fwrite(msg_queue.msg_body, 1, msg_queue.msg_len, fp_checkWav[thread_no]);
		//fflush(fp_checkWav[thread_no]);

		//越过开始时间之前的数据
		if (req->start_time >= start_time)
		{
			delete msg_queue.msg_body;
			msg_queue.msg_body = nullptr;
			start_time += msg_queue.msg_len / cnt_time;
			end_time = start_time;
			//int ret = asr_reconn_rtsp(thread_no, reconn_num, interval,E_BIZ_NAME_CHECK);
			continue;
		}

		if (rsp.result.start_time_stamp == 0)
		{
			rsp.result.start_time_stamp = msg_queue.msg_time;
		}

		/* 当前时间大于开始时间加上处理时间，则跳出循环*/
		if ( end_time > req->start_time+req->media_duration)
		{
			LOG(INFO)<<"msg time:["<<msg_queue.msg_time<<"],end_time["<<end_time<<"]";
			break;
		}

		/* 开始保存待识别数据 */
		if (arr == nullptr) {
			arr = new char[arr_len]();
		}
		if (cur_step + bufferSize>arr_len) {
			char* temp = new char[arr_len * 2]();
			memcpy(temp, arr, arr_len);
			arr_len *= 2;
			delete[] arr;
			arr = temp;
		}

		memcpy(arr + cur_step, msg_queue.msg_body, msg_queue.msg_len);
		cur_step += msg_queue.msg_len;
		end_time += msg_queue.msg_len / cnt_time;

		/* 每次拷贝完之后，要清除msg_queue new出来的内存，以避免内在泄漏 */
		if (nullptr != msg_queue.msg_body)
		{
			delete msg_queue.msg_body;
			msg_queue.msg_body = nullptr;
		}

		if (cur_step - arr_index >= bufferSize){
			/*条件符合是填充数据*/
			for (; arr_index + bufferSize < cur_step; arr_index += bufferSize){
				memcpy(buffer, arr + arr_index, bufferSize);
				Bytes2Shorts(buffer, bufferSize, sbuffer);
				Shorts2Floats(sbuffer, sbufferSize, fbuffer);
				mASC->FeedBuffer(fbuffer, sbufferSize);

				memset(buffer, 0x00, bufferSize);
				memset(sbuffer, 0x00, bufferSize >> 1);
				memset(fbuffer, 0x00, bufferSize >> 1);

				bc++;//填充次数			
			}

			//填充完毕将剩余语音移到buffer开始位置
			memcpy(buffer, buffer + arr_index, cur_step - arr_index);
			cur_step = cur_step - arr_index;			
			arr_index = 0;
		}

		/*符合条件则输出*/
		int cur_id = (end_time - req->start_time) / req->value_post_time;
		if ( cur_id != last_id){
			float res[3] = { 0.0 };
			int ret = mASC->PullResult(res);
			//rsp.result.start_time_stamp = msg_queue.msg_time - (int)(1875 * bufferSize * 1000.0 / 16000);
			rsp.result.start_time_stamp = (rsp.result.start_time_stamp > 0) ? start_time: 0;
			rsp.result.end_time_stamp = end_time;
			
			memcpy(rsp.task_id, req->task_id, strlen(req->task_id) + 1);
			rsp.sequence = seq_no++;
			rsp.status_code = E_STATUS_BIZ_NORMAL;
			rsp.result.Clip = res[0];
			rsp.result.Speech = res[1];
			rsp.result.Noise = res[2];
			memcpy(rsp.msg, "normal data!", strlen("normal data") + 1);
			noise_send_2_request(id, sock, rsp, req->task_id, E_MSG_TYPE_BIZ_NORMAL);
			cout << rsp.result.start_time_stamp << "\t" << rsp.result.end_time_stamp << "\t" << res[0] << "\t" << res[1] << "\t" << res[2] << endl;

			rsp.result.start_time_stamp = end_time;
			start_time = end_time;
			last_id = cur_id;
		}

		EnterCriticalSection(&g_cs_map_ctrl);
		auto it = g_map_ctrl.find(req->task_id);
		if (it != g_map_ctrl.end())
		{
			st = it->second;
			if (E_CTRL_CANCEL == st.ctrl_type)
			{
				end_flag = true;
				end_type = E_CTRL_CANCEL;
				st.ctrl_type = E_CTRL_SUCC;
				it->second = st;
			}
		}
		LeaveCriticalSection(&g_cs_map_ctrl);
	}//end of while(nRead == ...)

	delete[] sbuffer;
	delete[] fbuffer;

	LOG( INFO )<<"end  process voice check...";

	if (0 != hClient[thread_no])
	{
		int ret = CH_RTSP_Stop( hClient[thread_no] );
		if (0 == ret)
		{
			LOG(INFO )<< "rtsp stop success!thread_id["<< GetCurrentThreadId()<<"]";
		}
		else
		{
			LOG(INFO) << "rtsp stop failure!thread_id["<< GetCurrentThreadId()<<"]";
		}
	}

	hClient[thread_no] = 0;

	//fclose(fp_checkWav[thread_no]);

	if (E_CTRL_CANCEL == end_type)
	{
		VOICE_CHECK_RSP rsp;
		rsp.sequence = seq_no;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = end_time; //成功和失败时间都返回零
		noise_send_2_request(id, sock, rsp, req->task_id, E_MSG_TYPE_USER_CANCEL);
		LOG(INFO) << "voice check cancel success,task_id:" << req->task_id;
	}

	if (nullptr != mASC)
	{
		delete mASC;
		mASC = nullptr;
	}

	LOG( INFO)<<"Leave "<<__FUNCTION__" process...hClient["<<thread_no<<"]="<<hClient[thread_no];

	return;
}

/* 语音回调函数 */
int RTSP_AudioCallBackFunc( CH_RTSP_PCM_DATA rdcbd )
{
	int i = 0;
	int thread_no = 0;
	for (thread_no = 0;thread_no < Max_Threads;thread_no++)
	{
		if ( rdcbd.hRTSP == hClient[thread_no] )
		{
			break;
		}
	}
	
	if (UInitTimeSecond[thread_no] == 0 && UInitTimeUSecond[thread_no] == 0)
	{
		UInitTimeSecond[thread_no] = rdcbd.nTimestampSecond;
		UInitTimeUSecond[thread_no] = rdcbd.nTimestampUSecond;
		cout << "nTimestampSecond:" << rdcbd.nTimestampSecond << " nTimestampUSecond:" << rdcbd.nTimestampUSecond << endl;
	}
	
	//EnterCriticalSection(&g_csCheck[thread_no]); //进入临界区资源

	//fwrite(rdcbd.sData, 1, rdcbd.nDataLen, fp_checkWav[thread_no]);
	//fflush(fp_checkWav[thread_no]);

	/* 越界，达到最大值从零开始*/
	
	st_msg_queue msg_queue;
	//EnterCriticalSection(&g_callback_check);
	if (rdcbd.nTimestampSecond < UInitTimeSecond[thread_no])
	{
		i = (0xFFFFFFFF - UInitTimeSecond[thread_no]) * 1000 + rdcbd.nTimestampUSecond - UInitTimeUSecond[thread_no] + rdcbd.nTimestampSecond;
	}
	else
	{
		i = (rdcbd.nTimestampSecond - UInitTimeSecond[thread_no]) * 1000 + (rdcbd.nTimestampUSecond - UInitTimeUSecond[thread_no]);
	}

	//cout << "seconds:" << i << endl;

	msg_queue.msg_type = 1;
	msg_queue.end_flag = rdcbd.bIsEnd; //结束标志
	msg_queue.msg_time = i;
	//cout << "i:" << i << endl;

	msg_queue.msg_body = new char[rdcbd.nDataLen/2]();
	//::memcpy(msg_queue.msg_body, rdcbd.sData, rdcbd.nDataLen);
	for (UINT j = 0; j < rdcbd.nDataLen / 4; j++){
		((short*)msg_queue.msg_body)[j] = ((short*)rdcbd.sData)[j*2];
	}
	msg_queue.msg_len = rdcbd.nDataLen/2;

	//LeaveCriticalSection(&g_callback_check);

	g_msg_queue_check[thread_no].push(msg_queue); //把消息内容写到队列中
	//LeaveCriticalSection(&g_csCheck[thread_no]); //退出临界区资源

	return 0;
}

int ASR_Pull_RTSP_StreamCallBackFunc(CH_RTSP_PCM_DATA rdcbd)
{
	int i = 0;
	int thread_no = 0;
	for (thread_no = 0;thread_no < Max_Threads;thread_no++)
	{
		if (rdcbd.hRTSP == hAsrClient[thread_no])
		{
			break;
		}
	}

	if (asr_uinit_time_sec[thread_no] == 0 && asr_uinit_time_usec[thread_no] == 0)
	{
		asr_uinit_time_sec[thread_no] = rdcbd.nTimestampSecond;
		asr_uinit_time_usec[thread_no] = rdcbd.nTimestampUSecond;
		cout << "nTimestampSecond:" << rdcbd.nTimestampSecond << " nTimestampUSecond:" << rdcbd.nTimestampUSecond << endl;
	}

	st_msg_queue msg_queue = { 0 };
	
	i = (rdcbd.nTimestampSecond - asr_uinit_time_sec[thread_no]) * 1000 + (rdcbd.nTimestampUSecond - asr_uinit_time_usec[thread_no]);
	
	msg_queue.msg_type = thread_no;
	msg_queue.end_flag = rdcbd.bIsEnd; //结束标志0:未结束，1结束
	msg_queue.msg_time = i;

	msg_queue.msg_body = new char[rdcbd.nDataLen / 2]();
	//::memcpy(msg_queue.msg_body, rdcbd.sData, rdcbd.nDataLen);
	for (UINT j = 0; j < rdcbd.nDataLen / 4; j++) {
		((short*)msg_queue.msg_body)[j] = ((short*)rdcbd.sData)[j * 2];
	}
	msg_queue.msg_len = rdcbd.nDataLen / 2;

	g_msg_queue_asr[thread_no].push(msg_queue);

	return 0;
}

/* id_num 传进来的线程id，可以根据不同提线程创建不同的音频存储文件，使用完毕之后删除 */
void GetRtspUrlData::asr_speech_recognition(zframe_t *id, AUDIO_DETECTOR_REQ *req, void *sock, int thread_no)
{
	LOG( INFO)<<"Enter "<<__FUNCTION__<<" process!"<<thread_no;

	char fileName[FILE_NAME_LEN] = { 0 };
	sprintf(fileName, "rtsp_asr_%s_%d.pcm", req->task_id, thread_no);
	//char *fileName = "rtsp-vad-asr.wav";
	fp_asrWav[thread_no] = fopen(fileName, "ab+");
	if (nullptr == (fp_asrWav[thread_no]))
	{
		LOG(INFO) << "open rtsp_asr_" << req->task_id << "_" << thread_no << ".pcm fail!";
	}
	
	char record[FILE_NAME_LEN] = { 0 };
	sprintf(record, "rtsp_asr_vad_%s_%d.pcm", req->task_id, thread_no);
	fp_recog[thread_no]= fopen(record, "ab+");
	if (nullptr == (fp_recog[thread_no]))
	{
		LOG(INFO)<< "open rtsp_asr_vad_"<<req->task_id<<"_"<<thread_no<<".pcm fail!";
	}

	reset_init_time(thread_no, E_BIZ_NAME_ASR);
	clear_msg_list(thread_no, E_BIZ_NAME_ASR);

	int play_seconds = req->start_time * 1000; /* 传来的数据是毫秒，SDK需要的时间单位是秒 */
	int reconn_num = (req->reconn_num == 0 ? 2 : req->reconn_num);
	int interval = (req->reconn_interval == 0 ? 10 : req->reconn_interval);
	hAsrClient[thread_no] = CH_RTSP_Start(m_aRtspUrl, E_RTSP_CONN_MODE_TCP, 8000, play_seconds, reconn_num, interval, req->file_path);
	if (0 == hAsrClient[thread_no])
	{
		LOG(INFO)<<"ASR start rtsp error!thread_id["<<GetCurrentThreadId()<<"]，thread_no["<<thread_no<<"]";
	}

	CH_RTSP_SetAudioCallBackFunc(hAsrClient[thread_no], ASR_Pull_RTSP_StreamCallBackFunc, NULL);
	//CH_RTSP_SetStatusCallBackFunc(hAsrClient[thread_no], RTSP_StatusCallBackFunc, NULL);

	VoiceActivityDetector *vad = new VoiceActivityDetector();

	const int bufferSize = 800; //50ms数据
	int len = bufferSize >> 1;
	char *buffer = new char[bufferSize]();
	short* sBuffer = new short[bufferSize >> 1];
	unsigned int cnt_time = (8000 / 1000) * 16 / 8;

	AUDIO_DETECTOR_RSP *rsp = new AUDIO_DETECTOR_RSP;
	::memcpy(rsp->task_id, req->task_id, strlen(req->task_id) + 1);

	int seq_no = 0; //序列号
	bool isSpeech = false;

	UINT end_time = req->start_time + req->duration;
	UINT start_time = 0;
	UINT cur_end_time = 0;

	char *arr = nullptr;
	int arr_len = 160000;//初始值10second
	int arr_index = 0;
	int cur_step = 0;
	//int cur_time = 0;
	VADEvent lastEvent = DETECTOR_EVENT_NONE;

	int timeout = 0;

	const int  blank_len = bufferSize * 100;
	char blank_buf[blank_len] = { 0 }; /* 数据切分时使用，插入空白数据 */

	int end_type = 0;
	
	int end_flag = false;
	while (!end_flag)
	{
		if (g_msg_queue_asr[thread_no].empty() || g_msg_queue_asr[thread_no].size()==0)
		{
			if (E_RTSP_STATUS_RECON_SUCC == rtsp_asr_status[thread_no] || E_RTSP_STATUS_CONN_SUCC == rtsp_asr_status[thread_no])
			{
				if (req->start_time > start_time)
				{
					CH_RTSP_Stop(hClient[thread_no]);
					hClient[thread_no] = CH_RTSP_Start(m_aRtspUrl, E_RTSP_CONN_MODE_TCP, 8000, play_seconds, reconn_num, interval, req->file_path);
					CH_RTSP_SetAudioCallBackFunc(hAsrClient[thread_no], ASR_Pull_RTSP_StreamCallBackFunc, NULL);
					CH_RTSP_SetStatusCallBackFunc(hAsrClient[thread_no], RTSP_AsrStatusCallBackFunc, NULL);
				}
				timeout = 0;
				continue;
			}
			local_sleep(interval);
			if (timeout >= reconn_num)
			{
				end_flag = true;
				vad_send_2_request(id, sock, *rsp, req->task_id, E_MSG_TYPE_CONN_ERROR);
				LOG(INFO) << "timeout times bigger than reconnect number!";
			}
			timeout++;
			continue;
		}
		timeout = 0;

		st_msg_queue msg_queue = g_msg_queue_asr[thread_no].front(); //取头元素
		g_msg_queue_asr[thread_no].pop();                            //删除队头，指针后面释放

		int play_flag = msg_queue.end_flag;
		if ( E_PLAY_OVER == play_flag )
		{
			LOG(INFO)<< "the video stream is over,end flags["<<play_flag<<"]!";
			vad_send_2_request(id, sock, *rsp, req->task_id, E_MSG_TYPE_STREAM_OVER);
			break;
		}

		/* 把数据保存到文件中,该文件主要保存切分后的数据 */
		fwrite(msg_queue.msg_body, 1, msg_queue.msg_len, fp_recog[thread_no]);
		fflush(fp_recog[thread_no]);

		/* 把转换为单通道之后的数据保存下来*/
		fwrite(msg_queue.msg_body, 1, msg_queue.msg_len, fp_asrWav[thread_no]);
		fflush(fp_asrWav[thread_no]);

		/* 开始时间大于音频当前时间 */
		if (req->start_time > start_time)
		{
			/* 由于传来的时间是一段音频的结束时时间,也既下一段的开始时间，故在此计个基时间，以供下面计算所有*/
			start_time = msg_queue.msg_len / cnt_time;
			cur_end_time = start_time;
			delete msg_queue.msg_body;
			msg_queue.msg_body = nullptr;
			continue;
		}

		/* 开始保存待识别数据 */
		if (arr == nullptr){
			arr = new char[arr_len]();
			rsp->result.start_time_stamp = start_time;
		}
		if ( cur_step + bufferSize > arr_len ){
			char* temp = new char[arr_len * 2]();
			memcpy(temp, arr, arr_len);
			arr_len *= 2;
			delete[] arr;
			arr = temp;
		}

		LOG(INFO) << "Begin task_id:" << req->task_id << " thread_no:" << thread_no << " msg_len:" << msg_queue.msg_len;
		/* 缓存所有的数据，以避免漏掉数据 */
		memcpy(arr + cur_step, msg_queue.msg_body, msg_queue.msg_len);
		LOG(INFO) << "End task_id:" << req->task_id << " thread_no:" << thread_no << " msg_len:" << msg_queue.msg_len;

		/* 每次拷贝完之后，要清除msg_queue new出来的内存，以避免内在泄漏 */
		if( nullptr != msg_queue.msg_body)
		{ 
			delete msg_queue.msg_body;
			msg_queue.msg_body = nullptr;
		}

		cur_end_time += msg_queue.msg_len / cnt_time;

		cur_step += msg_queue.msg_len;
		rsp->result.end_time_stamp = cur_end_time;
		
		if (cur_step - arr_index <= bufferSize) //为了凑够50ms数据
		{
			continue;
		}

		memcpy(buffer, arr + arr_index, bufferSize );
		arr_index += bufferSize;

		vad->Bytes2Shorts(buffer, bufferSize, sBuffer);
		memset(buffer, 0x00, bufferSize);

		VADEvent currEvent = vad->Process(sBuffer, bufferSize / 2); /* 需要修改cur_step*/
		switch (currEvent)
		{
		case DETECTOR_EVENT_ACTIVITY: /* 语音开始,则开始缓存数据 */
			isSpeech = true;	
			lastEvent = DETECTOR_EVENT_ACTIVITY;
			break;

		case DETECTOR_EVENT_INACTIVITY:  /* 语音结束,则把缓存到的数据发送到asr进行识别 */
			/*语音或静音都要识别*/
			/*语音：只要是语音就必须识别，且以arr中录音为主*/
			/*非语音：超过10秒，调用一次识别请求*/

			/* 切分后的一段数据保存起来 中间插入一段空白数据 */
			fwrite(blank_buf, 1, blank_len, fp_recog[thread_no]);
			fflush(fp_recog[thread_no]);

			/*语音*/
			EnterCriticalSection(&g_csAsr[thread_no]);
			ASRService(id, sock, req, rsp, start_time, &seq_no, arr, &cur_step, &arr_index, isSpeech,thread_no);
			LeaveCriticalSection(&g_csAsr[thread_no]);

			isSpeech = false;
			lastEvent = DETECTOR_EVENT_ACTIVITY;
			break;
		default:
			/* 大于10s的录音 1s=128000b =16k,如果当前是在录音内，且语音超过20s，则截断，识别，避免内在溢出 */
			if ((isSpeech == true && cur_step >= 16000 * 20) || (isSpeech == false && cur_step >= 16000 * 10))//语音20秒,静音满10秒则请求识别 
			{
				EnterCriticalSection(&g_csAsr[thread_no]);
				ASRService(id, sock, req, rsp, start_time, &seq_no, arr, &cur_step, &arr_index, isSpeech, thread_no);
				LeaveCriticalSection(&g_csAsr[thread_no]);

				/* 切分后的一段数据保存起来 中间插入一段空白数据 */
				fwrite(blank_buf, 1, blank_len, fp_recog[thread_no]);
				fflush(fp_recog[thread_no]);
			}
			break;
		}//end of switch()

		start_time = cur_end_time;

		/* 音频间隔时长大于当前获得的时间 */
		if (cur_end_time > end_time)
		{
			if (cur_step > 0){
				EnterCriticalSection(&g_csAsr[thread_no]);
				ASRService(id, sock, req, rsp, start_time, &seq_no, arr, &cur_step, &arr_index, isSpeech, thread_no);
				LeaveCriticalSection(&g_csAsr[thread_no]);
			}
			LOG(INFO)<< "msg time["<<msg_queue.msg_time<<"],end time["<<req->start_time+req->duration<<"]";
			break;
		}

		/* 任务控制,如果控制类型为取消，则取消当前任务 */
		EnterCriticalSection(&g_cs_map_ctrl);
		auto it = g_map_ctrl.find(req->task_id);
		if (it != g_map_ctrl.end())
		{
			st_task_ctrl st;
			st = it->second;
			if (E_CTRL_CANCEL == st.ctrl_type)
			{
				end_flag = true;
				end_type = E_CTRL_CANCEL;
				st.ctrl_type = E_CTRL_SUCC;
				it->second = st;
				LOG(INFO) << "Get task control cancel,task_id:" << req->task_id;
			}
		}
		LeaveCriticalSection(&g_cs_map_ctrl);

	} //end of while()

	if (cur_step > 0){
		EnterCriticalSection(&g_csAsr[thread_no]);
		ASRService(id, sock, req, rsp, start_time, &seq_no, arr, &cur_step, &arr_index, isSpeech, thread_no);
		LeaveCriticalSection(&g_csAsr[thread_no]);
	}

	LOG(INFO) << "out of while process";

	if (0 != hAsrClient[thread_no])
	{
		int ret = CH_RTSP_Stop(hAsrClient[thread_no]);
		if (0 == ret)
		{
			LOG(INFO)<<"rtsp stop success!thread_id["<<GetCurrentThreadId()<<"]";
		}
		else
		{
			LOG(INFO)<< "rtsp stop failure!thread_id["<<GetCurrentThreadId()<<"]";
		}
	}
	hAsrClient[thread_no] = 0;

	fclose(fp_asrWav[thread_no]);
	fclose(fp_recog[thread_no]);

	if (E_CTRL_CANCEL == end_type)
	{
		rsp->sequence = seq_no;
		rsp->result.start_time_stamp = rsp->result.end_time_stamp = cur_end_time; //成功和失败时间都返回零
		vad_send_2_request(id, sock, *rsp, req->task_id, E_MSG_TYPE_USER_CANCEL );
		LOG(INFO) << "speech recongnize module:task cancel success,task_id:" << req->task_id;
	}

	delete rsp;
	rsp = nullptr;

	delete buffer;
	buffer = nullptr;
	delete sBuffer;
	sBuffer = nullptr;
	if (arr != nullptr){
		delete[] arr;
		arr = nullptr;
	}

	LOG( INFO)<<"Leave "<<__FUNCTION__<<" process...";

	return;
}

void GetRtspUrlData::ASRService(zframe_t *id, void *sock, AUDIO_DETECTOR_REQ *req, AUDIO_DETECTOR_RSP *rsp, int cur_time, int *seq_no, char* arr, int *cur_step, int *arr_index, bool isSpeech,int thread_no)
{
	char textBuf[1024] = { 0 };
	int outLen;

	if (0 == strlen(rsp->task_id))
	{
		memcpy(rsp->task_id, req->task_id, strlen(req->task_id) + 1);
	}

	if (g_asr_serv[0] != 0)
	{
		GetTextFromASR(g_asr_serv, arr, *cur_step, textBuf, outLen,thread_no);
	}
	else
	{
		GetTextFromASR("tcp://127.0.0.1:5859", arr, *cur_step, textBuf, outLen, thread_no);
	}
	
	cout << "isSpeech=" << isSpeech << "\t" << rsp->result.start_time_stamp << "\t" << rsp->result.end_time_stamp << "\t" << *cur_step*1000.0 / 16000 << "\t" << textBuf << endl;
	
	rsp->sequence = *seq_no;
	rsp->status_code = E_STATUS_BIZ_NORMAL;
	::memcpy(rsp->msg, "business normal", strlen("business normal") + 1);
	::memcpy(rsp->result.text, textBuf, outLen);
	
	cout << "the size of text is:" << strlen(rsp->result.text) << endl;

	vad_send_2_request(id, sock, *rsp, req->task_id, E_MSG_TYPE_BIZ_NORMAL);

	//发送完毕之后，要恢复相应的状态
	::memset(rsp, 0x00, sizeof(rsp->msg));

	/*清理缓冲区数据，清零相关计数器*/
	::memset(arr, 0x00, *cur_step);
	*cur_step = *arr_index = 0;
	rsp->result.start_time_stamp = cur_time;

	*seq_no = *seq_no + 1;

	return;
}

/* ASR服务器语音识别
* addr是asr服务器的地址端口信息
* waveStream是转入的音频信息
* buf是识别出的语音文本信息
*/
void GetRtspUrlData::GetTextFromASR( char *addr, char *waveStream,int waveLen, char *buf,int &bufLen,int thread_no )
{
	LOG( INFO)<<"Enter "<<__FUNCTION__<<" process..." ;
	void *zmq_ctx = zmq_ctx_new();
	void *client_socket = zmq_socket(zmq_ctx, ZMQ_REQ);

	zmq_connect(client_socket, addr);

	//prepare request	
	char* json_temp = "{\"appid\":\"lingban_hxdt\",\"key\":\"00000000-1111-2222-3333-888888888888\"}";
	int msg_size = strlen(json_temp) + 1;
	
	zmq_msg_t msg;
	int rc = zmq_msg_init_size(&msg, msg_size);
	memcpy(zmq_msg_data(&msg), json_temp, msg_size);
	rc = zmq_msg_send(&msg, client_socket, ZMQ_SNDMORE);
	
	CWaveHeader wavheader;
	wavheader.dwRIFFLen += waveLen;
	wavheader.dwdataLen += waveLen;
	wavheader.WaveFormat.wFormatTag = 1;
	wavheader.WaveFormat.nChannels = 1;
	wavheader.WaveFormat.nSamplesPerSec = 8000;
	wavheader.WaveFormat.nBlockAlign = 2;
	wavheader.WaveFormat.wBitsPerSample = 16;
	wavheader.WaveFormat.cbSize = 0;
	wavheader.WaveFormat.nAvgBytesPerSec = wavheader.WaveFormat.nSamplesPerSec*(wavheader.WaveFormat.wBitsPerSample/8);

	int wav_buffer_len = waveLen + 46;
	char * wav_buffer = new char[wav_buffer_len];
	int idx = 0;
	memcpy(wav_buffer+idx, &wavheader.dwRIFF, 4); idx += 4;
	memcpy(wav_buffer + idx, &wavheader.dwRIFFLen, 4); idx += 4;
	memcpy(wav_buffer + idx, &wavheader.dwWAVE, 4); idx += 4;
	memcpy(wav_buffer + idx, &wavheader.dwfmt, 4); idx += 4;
	memcpy(wav_buffer + idx, &wavheader.dwfmtLen, 4); idx += 4;
	memcpy(wav_buffer + idx, &wavheader.WaveFormat.wFormatTag, 2); idx += 2;
	memcpy(wav_buffer + idx, &wavheader.WaveFormat.nChannels, 2); idx += 2;
	memcpy(wav_buffer + idx, &wavheader.WaveFormat.nSamplesPerSec, 4); idx += 4;
	memcpy(wav_buffer + idx, &wavheader.WaveFormat.nAvgBytesPerSec, 4); idx += 4;
	memcpy(wav_buffer + idx, &wavheader.WaveFormat.nBlockAlign, 2); idx += 2;
	memcpy(wav_buffer + idx, &wavheader.WaveFormat.wBitsPerSample, 2); idx += 2;
	memcpy(wav_buffer + idx, &wavheader.WaveFormat.cbSize, 2); idx += 2;
	memcpy(wav_buffer + idx, &wavheader.dwdata, 4); idx += 4;
	memcpy(wav_buffer + idx, &wavheader.dwdataLen, 4); idx += 4;
	memcpy(wav_buffer + idx, waveStream, waveLen);

	zmq_msg_t msg_wav;
	rc = zmq_msg_init_size(&msg_wav, wav_buffer_len);
	memcpy(zmq_msg_data(&msg_wav), wav_buffer, wav_buffer_len);
	rc = zmq_msg_send(&msg_wav, client_socket, 0);

	zmq_msg_close(&msg);
	zmq_msg_close(&msg_wav);
	delete[] wav_buffer;

	//receive response
	zmq_msg_t msg_recv;
	rc = zmq_msg_init(&msg_recv);
	rc = zmq_msg_recv(&msg_recv, client_socket, 0);
	msg_size = zmq_msg_size(&msg_recv);
	memcpy(buf, zmq_msg_data(&msg_recv), msg_size);

	string result = decode_data_from_asr(buf);
	//ConvertAndCopy((char*)result.c_str(), (char*)recog_result.c_str(), bufLen, EncodingConversionType::UTF8_GB);
	//ConvertAndCopy((char*)result.c_str(), buf, bufLen, EncodingConversionType::UTF8_GB);
	memcpy(buf, result.c_str(), result.length()+1);
	//bufLen = strlen(buf)+1;
	bufLen = result.length()+1;
	zmq_msg_close(&msg_recv);

	LOG( INFO )<<"Leave "<<__FUNCTION__" process...";

	zmq_close(client_socket);
	zmq_ctx_destroy(zmq_ctx);
	return;
}

void GetRtspUrlData::clear_msg_list(int thread_no, E_BIZ_NAME biz_name )
{
	if (E_BIZ_NAME_CHECK == biz_name)
	{
		cout << "queue size:" << g_msg_queue_check[thread_no].size() << endl;

		//清空，为了避免之前已经有的数据未清空
		while (!g_msg_queue_check[thread_no].empty())
		{
			st_msg_queue msg_queue = g_msg_queue_check[thread_no].front(); //取头元素
			g_msg_queue_check[thread_no].pop(); //删除队头，指针后面释放
			if (nullptr != msg_queue.msg_body)
			{
				delete msg_queue.msg_body;
				msg_queue.msg_body = nullptr;
			}
		}
	}
	else
	{
		cout << "queue size:" << g_msg_queue_asr[thread_no].size() << endl;

		//清空
		while (!g_msg_queue_asr[thread_no].empty())
		{
			st_msg_queue msg_queue = g_msg_queue_asr[thread_no].front(); //取头元素
			g_msg_queue_asr[thread_no].pop(); //删除队头，指针后面释放
			if (nullptr != msg_queue.msg_body)
			{
				delete msg_queue.msg_body;
				msg_queue.msg_body = nullptr;
			}
		}
	}
	return;
}

void GetRtspUrlData::reset_init_time(int thread_no, E_BIZ_NAME biz_name)
{
	if (E_BIZ_NAME_CHECK == biz_name)
	{
		UInitTimeSecond[thread_no] = 0;
		UInitTimeUSecond[thread_no] = 0;
	}
	else
	{
		asr_uinit_time_sec[thread_no] = 0;
		asr_uinit_time_usec[thread_no] = 0;
	}
}

int GetRtspUrlData::asr_reconn_rtsp( int thread_no, int reconn_count, int reconn_interval, E_BIZ_NAME biz_name)
{
	int ret = -1;
	
	for (int cur_cnt = 0;cur_cnt < reconn_count;cur_cnt++)
	{
		if (E_RTSP_STATUS_RECON_SUCC == rtsp_status[thread_no] || E_RTSP_STATUS_CONN_SUCC==rtsp_status[thread_no])
		{
			ret = 0;
			break;
		}
		if (10 == reconn_interval)
		{
			local_sleep(10);
		}
		else 
		{
			local_sleep(reconn_interval/1000);
		}
	}

	return ret;
}

int GetRtspUrlData::check_task_ctrl( char *task_id )
{
	int ret = 0;
	EnterCriticalSection(&g_cs_map_ctrl);
	auto it = g_map_ctrl.find(task_id);
	if (it != g_map_ctrl.end())
	{
		st_task_ctrl st;
		st = it->second;
		if (E_CTRL_CANCEL == st.ctrl_type)
		{
			st.ctrl_type = E_CTRL_SUCC;
			it->second = st;
			ret =  E_CTRL_CANCEL;
		}
	}
	LeaveCriticalSection(&g_cs_map_ctrl);
	return ret;
}