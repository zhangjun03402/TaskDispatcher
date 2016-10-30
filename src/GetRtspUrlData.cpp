#include "..\inc\GetRtspUrlData.h"
#include "..\inc\CommonFunc.h"
#include "..\inc\AudioStateChecker.h"
#include "..\inc\TaskControl.h"
#include "..\inc\TaskDispatch.h"
#include "..\inc\VoiceActivityDetector.h"
#include "..\inc\zhelpers.h"
#include "..\inc\PublicHead.h"

#include "glog\logging.h"

#include "code.h"
#include "GlobalSettings.h"
#include "MessageParser.h"
#include <fstream>

#include <map>
#include <string>
#include <iostream>

#include <thread>
#include <ThreadSafeQueue.h>

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

extern CRITICAL_SECTION g_csCheck[Max_Threads];  //������������ٽ���

extern threadsafe_queue<st_msg_queue>	g_msg_queue_check[Max_Threads];
extern threadsafe_queue< st_msg_queue>	g_msg_queue_asr[Max_Threads];

extern mutex				g_mtxCheckMsgQ[Max_Threads];
extern mutex				g_mtxMapCtrl;
extern condition_variable	g_ConVar;

CH_RTSP_HANDLE hCheckClient[Max_Threads];
CH_RTSP_HANDLE hAsrClient[Max_Threads];

FILE* fp_checkWav[Max_Threads];
FILE* fp_asrWav[Max_Threads];
FILE *fp_recog[Max_Threads];

int rtsp_status[Max_Threads];
int rtsp_asr_status[Max_Threads];

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
	m_seq_no = 0;
}

GetRtspUrlData::~GetRtspUrlData()
{
}

int GetRtspUrlData::SetRtspUrl(char * rtspUrl)
{
	m_strRtspUrl = rtspUrl;
	return 0;
}

string GetRtspUrlData::GetRtspUrl()
{
	return m_strRtspUrl;
}

/* RTSP״̬�ṹ��
* @nStatusType
* 0 RTSP���ӳɹ�           1 RTSP����ʧ��
* 2 RTSP���Ӳ�����Ƶ����   3 ��ʼ����
* 4 RTSP�����ɹ�		   5 RTSP����ʧ��
* @hRTSP
* RTSP���
* @pContext
* �ص�������
*/
/* rtsp����״̬���»ص����� */
int RTSP_StatusCallBackFunc(CH_RTSP_STATUS rdcbd)
{
	cout << "Enter " << __FUNCTION__ << " process..." << endl;

	int thread_no = 0;
	for (thread_no = 0;thread_no < Max_Threads;thread_no++)
	{
		if ( rdcbd.hRTSPClient== hCheckClient[thread_no])
		{
			break;
		}
	}

	rtsp_status[thread_no] = rdcbd.nStatusType;
	LOG(INFO)<< "Enter "<<__FUNCTION__<<" process...,Check rtsp_status:"<< rtsp_status[thread_no];
	return rdcbd.nStatusType;
}

/* �ú�����ȡrtsp����״̬��ʵ��δʵ��(����ʶ��) */
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
	LOG(INFO) << "Enter " << __FUNCTION__ << " process...,ASR rtsp_asr_status:" << rtsp_asr_status[thread_no];
	return rdcbd.nStatusType;
}

/* ������ƣ�ʵ��ֻ��ɾ�� */
void GetRtspUrlData::UserCancelStream(zframe_t *id, TASK_CONTROL_INTERFACE &req, void *sock, int thread_no)
{
	TASK_CTRL_INTER_RSP rsp;
	memcpy(rsp.task_id, req.task_id, strlen(req.task_id) + 1);
	//st_task_ctrl st;
	
	bool bRet(false);
	CCtrlResponse rspCtrl(req.task_id);
	{
		std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
		bRet = g_ConVar.wait_for(lck, std::chrono::seconds(5),[=] {
			bool bFind(false);
			auto it = CGlobalSettings::mapTaskID2Ctrl.find(req.task_id);
			if (it != CGlobalSettings::mapTaskID2Ctrl.end())
			{
				if (E_CTRL_SUCC == it->second.ctrl_type)
				{
					bFind = true;
					CGlobalSettings::mapTaskID2Ctrl.erase(it);
				}
			}
			else
			{
				LOG(INFO) << "task control:doesn't find this task!task_id:" << req.task_id;
			}
			return bFind;
		});
	}
	if (bRet)
		rspCtrl.sendResponse(id, sock, "success");
	else
		rspCtrl.sendResponse(id, sock, "fail");
	
	//memcpy(rsp.result, "success", sizeof("success")); //����һ�����Ƴɹ�����Ϣ
	//send_rsp_2_task_ctrl(id, sock, rsp);

	//while (true)
	//{
	//	/* ��map�в��Ҹ�task_id��Ӧ������ */
	//	//EnterCriticalSection(&CGlobalSettings::csMapCtrl);
	//	std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
	//	auto it = CGlobalSettings::mapTaskID2Ctrl.find(req.task_id);
	//	if(it != CGlobalSettings::mapTaskID2Ctrl.end())
	//	{
	//		st = it->second;
	//		
	//		/* �����Ӧ��״̬Ϊ�ɹ�������һ�����Ƴɹ���Ϣ���ٰѸ�����Ϣɾ�� */
	//		if ( E_CTRL_SUCC== st.ctrl_type )
	//		{
	//			memcpy(rsp.result, "success", sizeof("success"));
	//			send_rsp_2_task_ctrl(id, sock, rsp);
	//			CGlobalSettings::mapTaskID2Ctrl.erase(it);
	//			//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
	//			lck.unlock();
	//			break;
	//		}
	//	}
	//	else /* ����ӣ����û�и�����id��Ӧ��map���ݣ���Ӧ���ظ�����һ����Ӧ�� */
	//	{
	//		LOG(INFO) << "task control:doesn't find this task!task_id:" << req.task_id;
	//		//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
	//		lck.unlock();
	//		break;
	//	}
	//	//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
	//	lck.unlock();
	//	local_sleep(1);
	//}
	std::cout<<"task cancel success!task_id:" << req.task_id;
	LOG(INFO) << "task control:business cancel success!task_id:" << req.task_id;
	return;
}

/*************************************************
Function:    asr_voice_quality_detection
Description: �������ʵ��ҵ�������
Input:       zframe_t *id ��Ϣ֡ͷ��Ϣ
VOICE_CHECK_REQ *req �����������ṹ��
void *sock  �������׽���
Output:      NULl
Return:      void
Others:      ��������ṹ���������ͼ������ʼʱ�䣬����
ʱ�䡣֮ǰ�������ͼ�������ݱ��浽vector������ʱ
�䵽��ʱ����vector���������ȫ�����ͳ�ȥ�����͹�
�Ĺ�ͨ����ֻ�������ͼ����һ�̼�⵽��ֵ
*************************************************/
bool GetRtspUrlData::asr_voice_quality_detection(zframe_t *id, VOICE_CHECK_REQ* req, void* sock, int thread_no)
{
	LOG( INFO )<<"Enter "<<__FUNCTION__<<" process...";

	//char fileName[FILE_NAME_LEN] = { 0 };
	//sprintf(fileName, "./store_wav/check_%s_%d.pcm", req->task_id, thread_no);
	//fp_checkWav[thread_no] = fopen(fileName, "ab+");

	int play_seconds = req->start_time * 1000; /* �����������Ǻ��룬SDK��Ҫ��ʱ�䵥λ���� */
	int reconn_num = (req->reconn_num == 0 ? 2 : req->reconn_num);
	int interval = (req->reconn_interval == 0 ? 10 : req->reconn_interval);

	unique_ptr<CVoiceResponse> upRspCheck = unique_ptr<CCheckResponse>(new CCheckResponse(req->task_id));

	/* new�����ʼ���� */
	unique_ptr<AudioStateChecker> mASC(new AudioStateChecker(8000, 512, 1875)); // 1875: 30s, 3750: 60s

	int seq_no = 0; //���к�
	int i = 0;

	const int bufferSize = 256;
	int sbufferSize = bufferSize >> 1;
	char buffer[bufferSize] = { 0 };
	short* sbuffer = new short[bufferSize >> 1];
	float* fbuffer = new float[bufferSize >> 1];

	/* ��ʼ������������Ӧ�Ĳ��� */
	hCheckClient[thread_no] = CH_RTSP_Start(m_strRtspUrl.c_str(), E_RTSP_CONN_MODE_TCP, 8000, play_seconds, reconn_num, interval, NULL/*req->file_path*/);
	//hCheckClient[thread_no] = CH_RTSP_Start( m_strRtspUrl.c_str(), E_RTSP_CONN_MODE_UDP, 8000 );
	if (0 == hCheckClient[thread_no])
	{
		LOG(INFO) << "voice check module:start rtsp error!thread_id[" << GetCurrentThreadId() << "], m_strRtspUrl=[" << m_strRtspUrl.c_str() << "]";
		//VOICE_CHECK_RSP rsp;
		//noise_send_2_request(id, sock, rsp, req->task_id, E_MSG_TYPE_START_RTSP_ERROR);
		cout << "voice check module:start rtsp error!" << endl;
		upRspCheck->sendResponse(id, sock, E_MSG_TYPE_START_RTSP_ERROR);
		return false;
	}

	//int nTimes = 0;
	//do {
	//	hCheckClient[thread_no] = CH_RTSP_Start(m_strRtspUrl.c_str(), E_RTSP_CONN_MODE_TCP, 8000);// , play_seconds, reconn_num, interval, req->file_path);
	//	if (0 == hCheckClient[thread_no])
	//	{
	//		if (2 == nTimes++)
	//		{
	//			cout << "Voice check module: restart rtsp error!" << endl;
	//			upRspCheck->sendResponse(id, sock, E_MSG_TYPE_START_RTSP_ERROR);
	//			return;
	//		}
	//		LOG(INFO) << "voice check module:start rtsp error!thread_id[" << GetCurrentThreadId() << "], m_strRtspUrl=[" << m_strRtspUrl.c_str() << "]";
	//		LOG(INFO) << "thread_id[ " << GetCurrentThreadId() << " ] sleep for 5 seconds to reconnect!";
	//		std::this_thread::sleep_for(std::chrono::seconds(5));
	//	}
	//} while (0 == hCheckClient[thread_no]);

	/* ��ʼ��ʱ��Ϊ0 */
	reset_init_time(thread_no, E_BIZ_NAME_CHECK);

	/* ���queue��������ݣ��Է�������������ɸ��� */
	clear_msg_list(thread_no, E_BIZ_NAME_CHECK);

	LOG(INFO) << "voice check module:start rtsp finish,handle:[" << hCheckClient[thread_no] << "]";

	/* ���û��Ľӿڣ�ע�������ʼ��������� */
	CH_RTSP_SetAudioCallBackFunc(hCheckClient[thread_no], RTSP_AudioCallBackFunc, NULL);
	/* ���û��Ľӿڣ�ע�������ʼ�����״̬���� */
	CH_RTSP_SetStatusCallBackFunc(hCheckClient[thread_no], RTSP_StatusCallBackFunc, NULL);

	int nRead = 0;
	int cnt_len = 0; //ͳ�Ƶ�ʱ�ܳ��ȣ�Ϊ��ͳ��ʱ������
	int cnt_push = 0;
	int push_count = 0; //�����ж��Ƿ���Ҫ���͵ļ�¼

	char *arr = nullptr;
	int arr_len = 16000;
	int arr_index = 0;
	int cur_step = 0;
	
	int bc = 0;
	int last_id = 0;

	VOICE_CHECK_RSP rsp = { 0 };

	LOG( INFO )<<"begin process voice check...";

	E_TASK_CONTROL end_type = E_CTRL_NONE;
	int end_flag = false; //������־
	int timeout = 0;
	int count = 0;
	unsigned int cnt_time = (8000/1000) * 16/8; /* ÿ��������ռ���ֽ��� */
	unsigned int start_time = 0;
	unsigned int end_time   = 0;


	while (!end_flag)
	{
		/* �������,�����������Ϊȡ������ȡ����ǰ���� */
		E_TASK_CONTROL ret = check_task_ctrl(req->task_id);
		if (E_CTRL_CANCEL == ret)
		{
			end_type = E_CTRL_CANCEL;
			LOG(INFO) << "voice check module:Get task control cancel,task_id:" << req->task_id;
			break;
		}
		//LOG(INFO) << "the " << ++count << " time(s) loop. msg queue size: " << g_msg_queue_check[thread_no].size();
		/* ȡqueue�е����� */
		//if (g_msg_queue_check[thread_no].empty())
		//{
		//	/* rtsp����״̬ */
		//	if (E_RTSP_STATUS_RECON_SUCC == rtsp_status[thread_no] || E_RTSP_STATUS_CONN_SUCC == rtsp_status[thread_no])
		//	{
		//		/* ���ݻ���Ҫ�������ǰʱ��С�ڿ�ʼʱ�䣬Ҫ����start������ע�ᡣʵ��״̬δʵ�� */
		//		if (req->start_time > start_time)
		//		{
		//			CH_RTSP_Stop(hCheckClient[thread_no]);
		//			hCheckClient[thread_no] = CH_RTSP_Start(m_strRtspUrl.c_str(), E_RTSP_CONN_MODE_TCP, 8000, play_seconds, reconn_num, interval, req->file_path);
		//			if (0 == hCheckClient[thread_no])
		//			{
		//				LOG(INFO) << "asr_voice_quality_detection re-start failed!";
		//				upRspCheck->sendResponse(id, sock, E_MSG_TYPE_START_RTSP_ERROR);
		//			}
		//			CH_RTSP_SetAudioCallBackFunc(hCheckClient[thread_no], RTSP_AudioCallBackFunc, NULL);
		//			CH_RTSP_SetStatusCallBackFunc(hCheckClient[thread_no], RTSP_StatusCallBackFunc, NULL);
		//		}
		//		timeout = 0;
		//		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		//		continue;
		//	}
		//	std::this_thread::sleep_for(std::chrono::seconds(interval));

		//	if (timeout >= reconn_num)
		//	{
		//		LOG(INFO)<< "reconnect rtsp fail!status["<< rtsp_status[thread_no]<<"]";
		//		end_flag = true;
		//		//VOICE_CHECK_RSP rsp;
		//		//noise_send_2_request(id, sock, rsp, req->task_id, E_MSG_TYPE_CONN_ERROR);
		//		upRspCheck->sendResponse(id, sock, E_MSG_TYPE_CONN_ERROR);
		//	}
		//	timeout++;
		//	continue;
		//}
		//timeout = 0;

		st_msg_queue msg_queue;
		bool bHave(false);
		bHave = g_msg_queue_check[thread_no].wait_and_pop(msg_queue, 2 * interval * 1000);  //��ת��Ϊ����, ���ȴ�2������ʱ�䣬rtsp�Լ�����������
		if (!bHave) {
			LOG(INFO) << "reconnect rtsp fail!status[" << rtsp_status[thread_no] << "]";
			end_flag = true;
			delete[] msg_queue.msg_body;
			msg_queue.msg_body = nullptr;
			upRspCheck->sendResponse(id, sock, E_MSG_TYPE_CONN_ERROR);
			continue;
		}

		/* ��ת��Ϊ��ͨ��֮������ݱ�������*/
		//fwrite(msg_queue.msg_body, 1, msg_queue.msg_len, fp_checkWav[thread_no]);
		//fflush(fp_checkWav[thread_no]);

		//Խ����ʼʱ��֮ǰ������
		if (req->start_time > start_time)
		{
			delete[] msg_queue.msg_body;
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

		/* ��ǰʱ����ڿ�ʼʱ����ϴ���ʱ�䣬������ѭ��*/
		if ( end_time > req->start_time+req->media_duration)
		{
			LOG(INFO)<<"msg time:["<<msg_queue.msg_time<<"],end_time["<<end_time<<"]";
			break;
		}

		/* ��ʼ�����ʶ������ */
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

		/* ÿ�ο�����֮��Ҫ���msg_queue new�������ڴ棬�Ա����ڴ�й© */
		if (nullptr != msg_queue.msg_body)
		{
			delete [] msg_queue.msg_body;
			msg_queue.msg_body = nullptr;
		}

		if (cur_step - arr_index >= bufferSize){
			/*��������ʱ�������*/
			for (; arr_index + bufferSize < cur_step; arr_index += bufferSize){
				memcpy(buffer, arr + arr_index, bufferSize);
				CGlobalSettings::Bytes2Shorts(buffer, bufferSize, sbuffer);
				CGlobalSettings::Shorts2Floats(sbuffer, sbufferSize, fbuffer);
				mASC->FeedBuffer(fbuffer, sbufferSize); 

				memset(buffer, 0x00, bufferSize);
				memset(sbuffer, 0x00, bufferSize >> 1);
				memset(fbuffer, 0x00, bufferSize >> 1);

				bc++;//������(������)			
			}

			//�����Ͻ�ʣ�������Ƶ�buffer��ʼλ��
			memcpy(buffer, buffer + arr_index, cur_step - arr_index);
			cur_step = cur_step - arr_index;			
			arr_index = 0;
		}

		/*�������������*/
		int cur_id = (end_time - req->start_time) / req->value_post_time;
		if ( cur_id != last_id || msg_queue.end_flag ){
			float res[3] = { 0.0 };
			int ret = mASC->PullResult(res);/* �����Ƶ����ز�����ֵ */
			//rsp.result.start_time_stamp = msg_queue.msg_time - (int)(1875 * bufferSize * 1000.0 / 16000);
			//rsp.result.start_time_stamp = (rsp.result.start_time_stamp > 0) ? start_time: 0;
			//rsp.result.end_time_stamp = end_time;
			
			//memcpy(rsp.task_id, req->task_id, strlen(req->task_id) + 1);
			/*rsp.sequence = seq_no++;
			rsp.result.Clip		= res[0];
			rsp.result.Speech	= res[1];
			rsp.result.Noise	= res[2];
			noise_send_2_request(id, sock, rsp, req->task_id, E_MSG_TYPE_BIZ_NORMAL);*/
			upRspCheck->start_time_stamp	= (rsp.result.start_time_stamp > 0) ? start_time : 0;
			upRspCheck->end_time_stamp		= end_time;
			upRspCheck->sequence			= seq_no++;
			upRspCheck->status_code			= E_STATUS_BIZ_NORMAL;
			upRspCheck->setCheckParameter(res[0], res[1], res[2]);	//clip, speech, noise;
			upRspCheck->sendResponse(id, sock, E_MSG_TYPE_BIZ_NORMAL);
			cout <<"start: " <<rsp.result.start_time_stamp << " end: " << end_time << ",  clip, speech, noise:" << res[0] << "\t" << res[1] << "\t" << res[2] << endl;
			//LOG(INFO) << "thread_no: ["<< thread_no<< "]  start: " << rsp.result.start_time_stamp << " end: " << end_time << ",  clip, speech, noise:" << res[0] << "\t" << res[1] << "\t" << res[2] << endl;

			rsp.result.start_time_stamp = end_time;
			start_time = end_time;
			last_id = cur_id;
			if (msg_queue.end_flag)
			{
				LOG(INFO) << "the end flag is: E_PLAY_OVER";
				break;	//�������һ֡���������������
			}
		}

	}//end of while(nRead == ...)

	delete[] arr;
	delete[] sbuffer;
	delete[] fbuffer;

	LOG( INFO )<<"end  process voice check...";

	if (0 != hCheckClient[thread_no])
	{
		/* �������� */
		int ret = CH_RTSP_Stop( hCheckClient[thread_no] );
		if (0 == ret)
		{
			hCheckClient[thread_no] = 0;
			LOG(INFO )<< "rtsp stop success!thread_id["<< GetCurrentThreadId()<<"]";
		}
		else
		{
			LOG(INFO) << "rtsp stop failure!thread_id["<< GetCurrentThreadId()<<"]";
		}
	}

	//fclose(fp_checkWav[thread_no]);

	if (E_CTRL_CANCEL == end_type)
	{
		assert(!hCheckClient[thread_no]);
		g_ConVar.notify_one();

		//VOICE_CHECK_RSP rsp = { 0 };
		//rsp.sequence = seq_no;
		//rsp.result.start_time_stamp = rsp.result.end_time_stamp = end_time;
		//noise_send_2_request(id, sock, rsp, req->task_id, E_MSG_TYPE_USER_CANCEL);
		
		upRspCheck->start_time_stamp= end_time;
		upRspCheck->end_time_stamp	= end_time;
		upRspCheck->sequence		= seq_no;
		upRspCheck->sendResponse(id, sock, E_MSG_TYPE_USER_CANCEL);

		LOG(INFO) << "Voice check cancel success,task_id:" << req->task_id;
	}

	LOG( INFO)<<"Leave "<<__FUNCTION__" process...hCheckClient["<<thread_no<<"]="<<hCheckClient[thread_no];

	return true;
}

/* �����ص����� */
int RTSP_AudioCallBackFunc( CH_RTSP_PCM_DATA rdcbd )
{
	int i = 0;
	int thread_no = 0;
	for (thread_no = 0;thread_no < Max_Threads;thread_no++)
	{
		if ( rdcbd.hRTSP == hCheckClient[thread_no] )
		{
			break;
		}
	}
	
	if (UInitTimeSecond[thread_no] == 0 && UInitTimeUSecond[thread_no] == 0)
	{
		UInitTimeSecond[thread_no] = rdcbd.nTimestampSecond;
		UInitTimeUSecond[thread_no] = rdcbd.nTimestampUSecond;
		cout << "Check nTimestampSecond:" << rdcbd.nTimestampSecond << " Check nTimestampUSecond:" << rdcbd.nTimestampUSecond << endl;
	}

	//EnterCriticalSection(&g_csCheck[thread_no]); //�����ٽ�����Դ

	//fwrite(rdcbd.sData, 1, rdcbd.nDataLen, fp_checkWav[thread_no]);
	//fflush(fp_checkWav[thread_no]);

	/* Խ�磬�ﵽ���ֵ���㿪ʼ*/
	
	st_msg_queue msg_queue = { 0 };
	if (rdcbd.nTimestampSecond < UInitTimeSecond[thread_no])
	{
		i = (0xFFFFFFFF - UInitTimeSecond[thread_no]) * 1000 + rdcbd.nTimestampUSecond - UInitTimeUSecond[thread_no] + rdcbd.nTimestampSecond;
	}
	else
	{
		i = (rdcbd.nTimestampSecond - UInitTimeSecond[thread_no]) * 1000 + (rdcbd.nTimestampUSecond - UInitTimeUSecond[thread_no]);
	}

	//cout << "seconds:" << i << endl;

	msg_queue.msg_type = 1; //����Ϊ1�ݶ�Ϊ����������⣬2�ݶ�Ϊ����ʶ��
	msg_queue.msg_id = thread_no; //msg_id�ݶ�Ϊ���̺�(�ǽ���id)
	msg_queue.end_flag = rdcbd.bIsEnd; //������־
	msg_queue.msg_time = i;
	//cout << "i:" << i << endl;

	msg_queue.msg_body = new char[rdcbd.nDataLen/2]();
	//::memcpy(msg_queue.msg_body, rdcbd.sData, rdcbd.nDataLen);
	for (UINT j = 0; j < rdcbd.nDataLen / 4; j++){
		((short*)msg_queue.msg_body)[j] = ((short*)rdcbd.sData)[j*2];
	}
	msg_queue.msg_len = rdcbd.nDataLen/2;
	g_msg_queue_check[thread_no].push(msg_queue); //����Ϣ����д��������

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
		cout << "ASR nTimestampSecond:" << rdcbd.nTimestampSecond << "ASR nTimestampUSecond:" << rdcbd.nTimestampUSecond << endl;
	}

	st_msg_queue msg_queue = { 0 };
	
	i = (rdcbd.nTimestampSecond - asr_uinit_time_sec[thread_no]) * 1000 + (rdcbd.nTimestampUSecond - asr_uinit_time_usec[thread_no]);
	
	msg_queue.msg_type = 2; //��Ϣ����:1�ݶ�Ϊ����������⣬2�ݶ�Ϊ����ʶ��
	msg_queue.msg_id = thread_no; //��Ϣid���ݶ�Ϊ���̺�(�ǽ���id)
	msg_queue.end_flag = rdcbd.bIsEnd; //������־0:δ������1����
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

/* id_num ���������߳�id�����Ը��ݲ�ͬ���̴߳�����ͬ����Ƶ�洢�ļ���ʹ�����֮��ɾ�� */
bool GetRtspUrlData::asr_speech_recognition(zframe_t *id, AUDIO_DETECTOR_REQ *req, void *sock, int thread_no)
{
	LOG( INFO)<<"Enter "<<__FUNCTION__<<" process!"<<thread_no;

	//char fileName[FILE_NAME_LEN] = { 0 };
	//sprintf(fileName, "rtsp_asr_%s_%d.pcm", req->task_id, thread_no);
	//fp_asrWav[thread_no] = fopen(fileName, "ab+");
	// (nullptr == (fp_asrWav[thread_no]))
	//
	//	LOG(INFO) << "open rtsp_asr_" << req->task_id << "_" << thread_no << ".pcm fail!";
	//}
	
	//char record[FILE_NAME_LEN] = { 0 };
	//sprintf(record, "rtsp_asr_vad_%s_%d.pcm", req->task_id, thread_no);
	//fp_recog[thread_no]= fopen(record, "ab+");
	//if (nullptr == (fp_recog[thread_no]))
	//{
	//	LOG(INFO)<< "open rtsp_asr_vad_"<<req->task_id<<"_"<<thread_no<<".pcm fail!";
	//}

	/* ��ʼ��ʱ��Ϊ0 */
	reset_init_time(thread_no, E_BIZ_NAME_ASR);

	/* ���queue��������ݣ��Է�������������ɸ��� */
	clear_msg_list(thread_no, E_BIZ_NAME_ASR);

	unique_ptr<VoiceActivityDetector> vad( new VoiceActivityDetector() );

	const int bufferSize = 800; //50ms����
	int len = bufferSize >> 1;
	char *buffer = new char[bufferSize]();
	short* sBuffer = new short[bufferSize >> 1];
	unsigned int cnt_time = (8000 / 1000) * 16 / 8;

	AUDIO_DETECTOR_RSP *rsp = new AUDIO_DETECTOR_RSP;
	::memcpy(rsp->task_id, req->task_id, strlen(req->task_id) + 1);

	int	 seq_no = 0; //���к�
	bool isSpeech = false;

	UINT end_time = req->start_time + req->duration;
	UINT start_time = 0;
	UINT cur_end_time = 0;

	char *arr = nullptr;
	int arr_len = 160000;//��ʼֵ10second
	int arr_index = 0;
	int cur_step = 0;
	//int cur_time = 0;
	VADEvent lastEvent = DETECTOR_EVENT_NONE;

	int timeout = 0;

	const int  blank_len = bufferSize * 100;
	char blank_buf[blank_len] = { 0 }; /* �����з�ʱʹ�ã�����հ����� */

	int play_seconds = req->start_time * 1000; /* �����������Ǻ��룬SDK��Ҫ��ʱ�䵥λ���� */
	int reconn_num = (req->reconn_num == 0 ? 2 : req->reconn_num);
	int interval = (req->reconn_interval == 0 ? 10 : req->reconn_interval);

	unique_ptr<CVoiceResponse> upRspRecog(new CRecogResponse(req->task_id));

	/* ��ʼ������������Ӧ�Ĳ��� */
	hAsrClient[thread_no] = CH_RTSP_Start(m_strRtspUrl.c_str(), E_RTSP_CONN_MODE_TCP, 8000, play_seconds, reconn_num, interval, req->file_path);
	if (0 == hAsrClient[thread_no])
	{
		LOG(INFO) << "speech recognize module:start rtsp error!thread_id[" << GetCurrentThreadId() << "]��thread_no[" << thread_no << "], m_strRtspUrl=["<< m_strRtspUrl.c_str() << "]";
		//vad_send_2_request(id, sock, *rsp, req->task_id, E_MSG_TYPE_START_RTSP_ERROR);
		//delete rsp;
		upRspRecog->sendResponse(id, sock, E_MSG_TYPE_START_RTSP_ERROR);
		return false;
	}

	/* ���û��Ľӿڣ�ע�������ʼ��������� */
	CH_RTSP_SetAudioCallBackFunc(hAsrClient[thread_no], ASR_Pull_RTSP_StreamCallBackFunc, NULL);
	CH_RTSP_SetStatusCallBackFunc(hAsrClient[thread_no], RTSP_AsrStatusCallBackFunc, NULL);

	int end_type = 0;
	int end_flag = false;
	bool bRet(false);
	while (!end_flag)
	{
		/* �������,�����������Ϊȡ������ȡ����ǰ���� */
		E_TASK_CONTROL ret = check_task_ctrl(req->task_id );
		if (E_CTRL_CANCEL == ret)
		{
			end_type = E_CTRL_CANCEL;
			LOG(INFO) << "speech recognize module:Get task control cancel,task_id:" << req->task_id;
			break;
		}
		//if (g_msg_queue_asr[thread_no].empty())
		//{
		//	if (E_RTSP_STATUS_RECON_SUCC == rtsp_asr_status[thread_no] || E_RTSP_STATUS_CONN_SUCC == rtsp_asr_status[thread_no])
		//	{
		//		if (req->start_time > start_time)
		//		{
		//			CH_RTSP_Stop(hAsrClient[thread_no]);
		//			hAsrClient[thread_no] = CH_RTSP_Start(m_strRtspUrl.c_str(), E_RTSP_CONN_MODE_TCP, 8000, play_seconds, reconn_num, interval, req->file_path);
		//			if (0 == hAsrClient[thread_no])
		//			{
		//				LOG(INFO) << "asr_speech_recognition re-registration failed!";
		//				upRspRecog->sendResponse(id, sock, E_MSG_TYPE_START_RTSP_ERROR);
		//				return false;
		//			}
		//			CH_RTSP_SetAudioCallBackFunc(hAsrClient[thread_no], ASR_Pull_RTSP_StreamCallBackFunc, NULL);
		//			CH_RTSP_SetStatusCallBackFunc(hAsrClient[thread_no], RTSP_AsrStatusCallBackFunc, NULL);
		//		}
		//		timeout = 0;
		//		LOG(INFO) << "msg queue empty, wait for stream! ";
		//		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		//		continue;
		//	}
		//	std::this_thread::sleep_for(std::chrono::seconds(interval));
		//	if (timeout >= reconn_num)
		//	{
		//		end_flag = true;
		//		upRspRecog->sendResponse(id, sock, E_MSG_TYPE_CONN_ERROR);
		//		//vad_send_2_request(id, sock, *rsp, req->task_id, E_MSG_TYPE_CONN_ERROR);
		//		LOG(INFO) << "timeout times bigger than reconnect number!";
		//	}
		//	timeout++;
		//	continue;
		//}
		//timeout = 0;

		st_msg_queue msg_queue;
		bool bHave(false);
		bHave = g_msg_queue_asr[thread_no].wait_and_pop(msg_queue, 2 * interval * 1000);  //��ת��Ϊ����, ���2������ʱ�䣬rtsp�Լ�����������
		if (!bHave) {
			LOG(INFO) << "reconnect rtsp fail!status[" << rtsp_asr_status[thread_no] << "]";
			end_flag = true;
			upRspRecog->sendResponse(id, sock, E_MSG_TYPE_CONN_ERROR);
			delete[] msg_queue.msg_body;
			msg_queue.msg_body = nullptr;
			continue;
		}

		/* �����ݱ��浽�ļ���,���ļ���Ҫ�����зֺ������ */
		//fwrite(msg_queue.msg_body, 1, msg_queue.msg_len, fp_recog[thread_no]);
		//fflush(fp_recog[thread_no]);

		/* ��ת��Ϊ��ͨ��֮������ݱ�������*/
		//fwrite(msg_queue.msg_body, 1, msg_queue.msg_len, fp_asrWav[thread_no]);
		//fflush(fp_asrWav[thread_no]);

		_ASSERTE(_CrtCheckMemory());

		/* ��ʼʱ�������Ƶ��ǰʱ�� */
		if (req->start_time > start_time)
		{
			/* ���ڴ�����ʱ����һ����Ƶ�Ľ���ʱʱ��,Ҳ����һ�εĿ�ʼʱ�䣬���ڴ˼Ƹ���ʱ�䣬�Թ������������*/
			start_time = msg_queue.msg_len / cnt_time;
			cur_end_time = start_time;
			delete[] msg_queue.msg_body;
			msg_queue.msg_body = nullptr;
			continue;
		}

		/* ��ʼ�����ʶ������ */
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

		/* �������е����ݣ��Ա���©������ */
		memcpy(arr + cur_step, msg_queue.msg_body, msg_queue.msg_len);

		/* ÿ�ο�����֮��Ҫ���msg_queue new�������ڴ棬�Ա�������й© */
		if( nullptr != msg_queue.msg_body)
		{ 
			delete[] msg_queue.msg_body;
			msg_queue.msg_body = nullptr;
		}
		_ASSERTE(_CrtCheckMemory());
		cur_end_time += msg_queue.msg_len / cnt_time;

		cur_step += msg_queue.msg_len;
		rsp->result.end_time_stamp = cur_end_time;
		
		if (cur_step - arr_index <= bufferSize) //Ϊ�˴չ�50ms����
		{
			continue;
		}

		memcpy(buffer, arr + arr_index, bufferSize );
		arr_index += bufferSize;

		CGlobalSettings::Bytes2Shorts(buffer, bufferSize, sBuffer);
		//vad->Bytes2Shorts(buffer, bufferSize, sBuffer);
		memset(buffer, 0x00, bufferSize);
		_ASSERTE(_CrtCheckMemory());
		VADEvent currEvent = vad->Process(sBuffer, bufferSize / 2); /* ��Ҫ�޸�cur_step*/
		switch (currEvent)
		{
		case DETECTOR_EVENT_ACTIVITY: /* ������ʼ,��ʼ�������� */
			isSpeech = true;	
			lastEvent = DETECTOR_EVENT_ACTIVITY;
			LOG(INFO) << "DETECTOR_EVENT_ACTIVITY";
			break;

		case DETECTOR_EVENT_INACTIVITY:  /* ��������,��ѻ��浽�����ݷ��͵�asr����ʶ�� */
			/*����������Ҫʶ��*/
			/*������ֻҪ�������ͱ���ʶ������arr��¼��Ϊ��*/
			/*������������10�룬����һ��ʶ������*/

			/* �зֺ��һ�����ݱ������� �м����һ�οհ����� */
			//fwrite(blank_buf, 1, blank_len, fp_recog[thread_no]);
			//fflush(fp_recog[thread_no]);

			/*����*/
			bRet = ASRService(id, sock, req, rsp, start_time, &seq_no, arr, &cur_step, &arr_index, isSpeech,thread_no);

			isSpeech = false;
			lastEvent = DETECTOR_EVENT_ACTIVITY;
			LOG(INFO) << "DETECTOR_EVENT_INACTIVITY";
			break;
		default:
			/* ����10s��¼�� 1s=128000b =16k,�����ǰ����¼���ڣ�����������20s����ضϣ�ʶ�𣬱���������� */
			if ((isSpeech == true && cur_step >= 16000 * 20) || (isSpeech == false && cur_step >= 16000 * 10))//����20��,������10��������ʶ�� 
			{
				bRet = ASRService(id, sock, req, rsp, start_time, &seq_no, arr, &cur_step, &arr_index, isSpeech, thread_no);

				/* �зֺ��һ�����ݱ������� �м����һ�οհ����� */
				//fwrite(blank_buf, 1, blank_len, fp_recog[thread_no]);
				//fflush(fp_recog[thread_no]);
			}
			break;
		}//end of switch()

		start_time = cur_end_time;
		_ASSERTE(_CrtCheckMemory());
		/* ��Ƶ���ʱ�����ڵ�ǰ��õ�ʱ�� */
		if (cur_end_time > end_time)
		{
			/*if (cur_step > 0){
				bRet = ASRService(id, sock, req, rsp, start_time, &seq_no, arr, &cur_step, &arr_index, isSpeech, thread_no);
				if (!bRet)
					goto clear;
			}*/
			LOG(INFO)<< "msg time["<<msg_queue.msg_time<<"],end time["<<req->start_time+req->duration<<"]";
			break;
		}

		if (E_PLAY_OVER == msg_queue.end_flag)
		{
			LOG(INFO) << "the video stream is over!";
			upRspRecog->sendResponse(id, sock, E_MSG_TYPE_STREAM_OVER);
			//vad_send_2_request(id, sock, *rsp, req->task_id, E_MSG_TYPE_STREAM_OVER);
			break;
		}
	} //end of while()

	if (cur_step > 0){
		bRet = ASRService(id, sock, req, rsp, start_time, &seq_no, arr, &cur_step, &arr_index, isSpeech, thread_no);
	}

	LOG(INFO) << "out of while process";

	if (0 != hAsrClient[thread_no])
	{
		int ret = CH_RTSP_Stop(hAsrClient[thread_no]);
		if (0 == ret)
		{
			hAsrClient[thread_no] = 0;
			LOG(INFO)<<"rtsp stop success!thread_id["<<GetCurrentThreadId()<<"]";
		}
		else
		{
			LOG(INFO)<< "rtsp stop failure!thread_id["<<GetCurrentThreadId()<<"]";
		}
	}

	//fclose(fp_asrWav[thread_no]);
	//fclose(fp_recog[thread_no]);

	//DeleteFile(fileName);
	//DeleteFile(record);

	if (E_CTRL_CANCEL == end_type)
	{
		g_ConVar.notify_one();
		//rsp->sequence = seq_no;
		//rsp->result.start_time_stamp = rsp->result.end_time_stamp = cur_end_time;
		//vad_send_2_request(id, sock, *rsp, req->task_id, E_MSG_TYPE_USER_CANCEL );
		upRspRecog->sequence			= seq_no;
		upRspRecog->start_time_stamp	= cur_end_time;
		upRspRecog->end_time_stamp		= cur_end_time;
		upRspRecog->sendResponse(id, sock, E_MSG_TYPE_USER_CANCEL);
		LOG(INFO) << "speech recongnize module:task cancel success,task_id:" << req->task_id;
	}

	delete rsp;
	rsp = nullptr;

	delete[] buffer;
	buffer = nullptr;
	delete[] sBuffer;
	sBuffer = nullptr;
	if (arr != nullptr){
		delete[] arr;
		arr = nullptr;
	}

	LOG( INFO)<<"Leave "<<__FUNCTION__<<" process...";

	return bRet;
}

/* ASRService����ʶ����ش���������������asr����ʶ������� */
bool GetRtspUrlData::ASRService(zframe_t *id, void *sock, AUDIO_DETECTOR_REQ *req, AUDIO_DETECTOR_RSP *rsp, int cur_time, int *seq_no, char* arr, int *cur_step, int *arr_index, bool isSpeech,int thread_no)
{
	char textBuf[1024] = { 0 };
	int outLen;
	bool bRet(false);

	if (0 == strlen(rsp->task_id))
	{
		memcpy(rsp->task_id, req->task_id, strlen(req->task_id) + 1);
	}

	string sAddr = CGlobalSettings::GetInstance().GetASRServerAddr();
	if (!sAddr.empty())
	{
		bRet = GetTextFromASR(sAddr.c_str(), arr, *cur_step, textBuf, outLen,thread_no);
	}
	else
	{
		/* ���������ݷ���ʶ����������ӻ��ʶ���� */
		bRet = GetTextFromASR("tcp://127.0.0.1:5859", arr, *cur_step, textBuf, outLen, thread_no);
	}
	if (!bRet)
		return false;
	
	cout << "isSpeech=" << isSpeech << "\t" << rsp->result.start_time_stamp << "\t" << rsp->result.end_time_stamp << "\t" << *cur_step*1000.0 / 16000 << "\t" << textBuf << endl;
	LOG(INFO) << "isSpeech:" << isSpeech<<" task_id:"<<rsp->task_id << " recognize result:" << textBuf;
	
	rsp->sequence = *seq_no;
	rsp->status_code = E_STATUS_BIZ_NORMAL;
	::memcpy(rsp->msg, "business normal", strlen("business normal") + 1);
	::memcpy(rsp->result.text, textBuf, outLen);
	
	cout << "the size of text is:" << strlen(rsp->result.text) << endl;

	/* ��ʶ������������ */
	vad_send_2_request(id, sock, *rsp, req->task_id, E_MSG_TYPE_BIZ_NORMAL);

	//�������֮��Ҫ�ָ���Ӧ��״̬
	::memset(rsp, 0x00, sizeof(rsp->msg));

	/*�����������ݣ�������ؼ�����*/
	::memset(arr, 0x00, *cur_step);
	*cur_step = *arr_index = 0;
	rsp->result.start_time_stamp = cur_time;

	*seq_no = *seq_no + 1;

	return true;
}

/* ASR����������ʶ��
* addr��asr�������ĵ�ַ�˿���Ϣ
* waveStream��ת�����Ƶ��Ϣ
* buf��ʶ����������ı���Ϣ
*/
bool GetRtspUrlData::GetTextFromASR( const char *addr, char *waveStream,int waveLen, char *buf,int &bufLen,int thread_no )
{
	//LOG( INFO)<<"Enter "<<__FUNCTION__<<" process..." ;
	void *zmq_ctx = zmq_ctx_new();
	void *client_socket = zmq_socket(zmq_ctx, ZMQ_REQ);
	if (!client_socket)
	{
		LOG(INFO) << "create socket to connect asr error,type is:ZMQ_REQ,errno:" << zmq_strerror(zmq_errno());
		return false;
	}

	int ret = zmq_connect(client_socket, addr);
	if (0 != ret)
	{
		LOG(INFO) << "line:" << __LINE__ << " connect asr recognize server error,errno:" << zmq_strerror(zmq_errno())<<" address:"<<addr;
		return false;
	}

	//prepare request	
	char* json_temp = "{\"appid\":\"lingban_hxdt\",\"key\":\"00000000-1111-2222-3333-888888888888\"}";
	int msg_size = strlen(json_temp) + 1;
	
	zmq_msg_t msg;
	int rc = zmq_msg_init_size(&msg, msg_size);
	memcpy(zmq_msg_data(&msg), json_temp, msg_size);
	rc = zmq_msg_send(&msg, client_socket, ZMQ_SNDMORE);
	
	/* ��������ͷ����Ϣ */
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
	if (-1 == rc)
	{
		LOG(INFO) << "init zmq size prepare to asr server error,errno:" << zmq_strerror(zmq_errno()) << " address:" << addr;
		return false;
	}

	memcpy(zmq_msg_data(&msg_wav), wav_buffer, wav_buffer_len);
	rc = zmq_msg_send(&msg_wav, client_socket, 0);
	if (-1 == rc)
	{
		LOG(INFO) <<"send voice data to asr server error,errno:" << zmq_strerror(zmq_errno()) << " address:" << addr;
		return false;
	}

	zmq_msg_close(&msg);
	zmq_msg_close(&msg_wav);
	delete[] wav_buffer;

	//receive response
	zmq_msg_t msg_recv;
	rc = zmq_msg_init(&msg_recv);
	int iRcvTimeout = 5000;// millsecond
	zmq_setsockopt(client_socket, ZMQ_RCVTIMEO, &iRcvTimeout, sizeof(iRcvTimeout));
	
	rc = zmq_msg_recv(&msg_recv, client_socket, 0);
	if (-1 == rc)
	{
		LOG(INFO) << "recv voice data from asr server error, errno(" << zmq_errno() << "): " << zmq_strerror(zmq_errno()) << "! Server address:" << addr;
		zmq_msg_close(&msg_recv);
		return false;
	}

	msg_size = zmq_msg_size(&msg_recv);
	if( msg_size > 0 )
	{ 
		memcpy(buf, zmq_msg_data(&msg_recv), msg_size);
		LOG(INFO) << "Recv from ASR server: " << buf;
		string result = decode_data_from_asr(buf);
		memcpy(buf, result.c_str(), result.length() + 1);
		bufLen = result.length() + 1;
	}
	else
	{
		LOG(INFO) << "recv voice data from asr server msg size is 0" ;
	}

	//string result = decode_data_from_asr(buf);
	////ConvertAndCopy((char*)result.c_str(), (char*)recog_result.c_str(), bufLen, EncodingConversionType::UTF8_GB);
	////ConvertAndCopy((char*)result.c_str(), buf, bufLen, EncodingConversionType::UTF8_GB);
	//memcpy(buf, result.c_str(), result.length()+1);
	////bufLen = strlen(buf)+1;
	//bufLen = result.length()+1;
	zmq_msg_close(&msg_recv);

	//LOG( INFO )<<"Leave "<<__FUNCTION__" process...";

	zmq_close(client_socket);
	zmq_ctx_destroy(zmq_ctx);
	return true;
}

void GetRtspUrlData::clear_msg_list(int thread_no, E_BIZ_NAME biz_name )
{
	if (E_BIZ_NAME_CHECK == biz_name)
	{
		//cout << "queue size:" << g_msg_queue_check[thread_no].size() << endl;

		//��գ�Ϊ�˱���֮ǰ�Ѿ��е�����δ���
		while (!g_msg_queue_check[thread_no].empty())
		{
			st_msg_queue msg_queue;
			if (!g_msg_queue_check[thread_no].try_pop(msg_queue))
				break;
			if (nullptr != msg_queue.msg_body)
			{
				delete[] msg_queue.msg_body;
				msg_queue.msg_body = nullptr;
			}
		}
	}
	else
	{
		//cout << "queue size:" << g_msg_queue_asr[thread_no].size() << endl;

		//���
		while (!g_msg_queue_asr[thread_no].empty())
		{
			st_msg_queue msg_queue;
			if (g_msg_queue_asr[thread_no].try_pop(msg_queue))
				break;
			if (nullptr != msg_queue.msg_body)
			{
				delete[] msg_queue.msg_body;
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

/* û��ʹ��*/
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

/* ���������� */
E_TASK_CONTROL GetRtspUrlData::check_task_ctrl( char *task_id )
{
	E_TASK_CONTROL ret(E_CTRL_NONE);
	st_task_ctrl st;

	//EnterCriticalSection(&CGlobalSettings::csMapCtrl);
	std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
	auto it = CGlobalSettings::mapTaskID2Ctrl.find(task_id);
	if (it != CGlobalSettings::mapTaskID2Ctrl.end())
	{
		st = it->second;
		if (E_CTRL_CANCEL == st.ctrl_type)
		{
			st.ctrl_type = E_CTRL_SUCC;
			it->second = st;
			ret =  E_CTRL_CANCEL;
		}
	}
	//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
	lck.unlock();

	return ret;
}