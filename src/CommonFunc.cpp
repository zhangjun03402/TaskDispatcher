/*****************************************************************************
Copyright: 2016-2016, LingBan Tech. Co., Ltd.
File name: CommonFunc.cpp
Description: ���ļ��к���Ϊ������Ҫʹ�õĺ���
Author: ����
Version: �汾
Date: �������
History: �޸���ʷ��¼�б� ÿ���޸ļ�¼Ӧ�����޸����ڡ��޸�
�߼��޸����ݼ�����
*****************************************************************************/
#include "..\inc\PublicHead.h"
#include "..\inc\CommonFunc.h"
#include "libczmq\include\czmq.h"
#include "json\json.h"
#include <sstream>
#include <time.h>
//#include "log.h"
#include <glog\logging.h>
#include <fstream>

//using namespace Lingban::Common;


/*************************************************
Function:    str_2_check_req 
Description: �ѽ��յ���json�ַ���������Ϊ�����������
     ����ṹ��
Input:  string str:���յ���json��
		VOICE_CHECK_REQ &req:���������������ṹ��
		��json���е���Ϣת����req�ṹ����ֶ�
Output: NULL
Return: void
Others: // ����˵��
*************************************************/
void str_2_check_req( string str, VOICE_CHECK_REQ &req)
{
	assert(!str.empty());
	Json::Reader reader;
	Json::Value root;
	if (reader.parse(str, root))
	{
		//Json::Value root_value = root["video_check_req"];

		string t_id = root["task_id"].asString();
		memcpy(req.task_id, t_id.c_str(), t_id.length()+1);

		string url = root["media_url"].asString();
		memcpy(req.media_url, url.c_str(), url.length()+1);

		req.start_time = root["start_time"].asInt();
		req.media_duration = root["duration"].asInt();
		req.value_post_time = root["value_post_time"].asInt();
		req.FS = root["FS"].asInt();
		req.FFTL = root["FFTL"].asInt();
		req.STAL = root["STAL"].asInt();
		req.reconn_num = root["reconn_num"].asInt();
		req.reconn_interval = root["reconn_interval"].asInt();
		string file_path = root["pcm_file_path"].asString();
		memcpy(req.file_path, file_path.c_str(), file_path.length()+1);
	}
	else
	{
		cout << "the string of request video check is null" << endl;
	}
	return;
}

/*************************************************
Function:    str_2_detector_req
Description: �ѽ��յ���json������������ʶ������ṹ����
Input:  string str:���յ���json��
AUDIO_DETECTOR_REQ &req:���������������ṹ��
��json���е���Ϣת����req�ṹ����ֶ�
Output: NULL
Return: void
Others: // ����˵��
*************************************************/
void str_2_detector_req(string str, AUDIO_DETECTOR_REQ &req)
{
	Json::Reader reader;
	Json::Value root;
	if (reader.parse(str, root))
	{
		//Json::Value root_value = root["audio_detector_req"];

		string t_id = root["task_id"].asString();
		memcpy(req.task_id, t_id.c_str(), t_id.length()+1);

		string url = root["media_url"].asString();
		memcpy(req.media_url, url.c_str(), url.length()+1);

		req.start_time = root["start_time"].asInt();
		req.duration = root["duration"].asInt();
		req.vad_state_duration = root["vad_state_duration"].asInt();
		req.reconn_num = root["reconn_num"].asInt();
		req.reconn_interval = root["reconn_interval"].asInt();
		string file_path = root["pcm_file_path"].asString();
		memcpy(req.file_path, file_path.c_str(), file_path.length() + 1);
	}
	else
	{
		cout << "the string of request audio detector is null" << endl;
	}
	return;
}

/*********************************************************
Function:    str_2_control_req
Description: �ѽ��յ���json�ַ���������Ϊ������ƽṹ����
����ṹ��
Input:  string str:���յ���json��
TASK_CONTROL_INTERFACE &req:���������������ṹ��
��json���е���Ϣת����req�ṹ����ֶ�
Output: NULL
Return: void
Others: // ����˵��
**********************************************************/
void decode_str_2_control_req(IN zframe_t *id_frame, IN Json::Value str, IN OUT TASK_CONTROL_INTERFACE &req)
{
	//string t_id = str["task_id"].asString();
	//memcpy(req.task_id, t_id.c_str(), t_id.length() + 1);
	//string flags = str["ctrl_flags"].asString();
	//memcpy(req.ctrl_flag, flags.c_str(), flags.length() + 1);
	//st_task_ctrl st;
	//if (stricmp(flags.c_str(), "cancel") == 0)
	//{
	//	st.ctrl_type = E_CTRL_CANCEL;
	//}

	//EnterCriticalSection(&CGlobalSettings::csMapCtrl);
	//auto it = g_map_ctrl.find(t_id);
	//if (it == g_map_ctrl.end())
	//{
	//	/*������Ҳ�����Ҫ����һ����ǰû�и�����Ĵ���*/

	//}
	//else
	//{
	//	/* �����ǰmap����Щ���ݣ���ѿ������͸�����Ӧ��ֵ */
	//	it->second = st;
	//}
	//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);

	return;
}

/*************************************************
Function:    voise_rsp_2_str
Description: ��voise��Ӧ��Ϣ��ת�����ַ�������
����ṹ��
Input:  VOICE_CHECK_REQ &req:���������������ṹ��
       ת����json��
Output: NULL
Return: void
Others: // ����˵��
*************************************************/
string noise_rsp_2_str(VOICE_CHECK_RSP &rsp)
{
	Json::Value root; //root node

	Json::Value item;
	root["task_id"] = rsp.task_id;
	root["message"] = rsp.msg;
	root["sequence"] = rsp.sequence;
	root["status_code"] = rsp.status_code;

	Json::Value result;
	result["start_time"] = rsp.result.start_time_stamp;
	result["end_time"] = rsp.result.end_time_stamp;
	result["Clip"] = rsp.result.Clip;
	result["Speech"] = rsp.result.Speech;
	result["Noise"] = rsp.result.Noise;
	root["result"] = result;

	string str = root.toStyledString();
	return str;
}

/*************************************************
Function:    detector_rsp_2_str
Description: �Ѵ��������Ӧ����Ϣת��Ϊjson��
Input:       AUDIO_DETECTOR_RSP rsp
Output:      string:ת�����json��
Return:      string
Others:      ��������ҵ�����Ӧ����Ϣ�ṹ��һ������
       ʹ�ô�һ���ӿ�
*************************************************/
string detector_rsp_2_str( AUDIO_DETECTOR_RSP &rsp )
{
	Json::Value root; //root node

	root["task_id"]     = rsp.task_id;
	root["message"]     = rsp.msg;
	root["sequence"]    = rsp.sequence;
	root["status_code"] = rsp.status_code;

	Json::Value result;
	result["start_time"] = rsp.result.start_time_stamp;
	result["end_time"]   = rsp.result.end_time_stamp;
	result["text"]       = rsp.result.text;
	root["result"]       = result;

	string str = root.toStyledString();
	return str;
}

/*  ���ַ���ת������ʶ��ṹ�� */
void decode_str_2_detector_req(Json::Value str, AUDIO_DETECTOR_REQ &req)
{
		//Json::Value root_value = root["audio_detector_req"];

	string t_id = str["task_id"].asString();
	memcpy(req.task_id, t_id.c_str(), t_id.length() + 1);

	string url = str["media_url"].asString();
	memcpy(req.media_url, url.c_str(), url.length() + 1);

	req.start_time = str["start_time"].asInt();
	req.duration = str["duration"].asInt();

	req.vad_state_duration = str["vad_state_duration"].asInt();
	req.level_threshold    = str["level_threshold"].asInt();
	req.level_current      = str["level_current"].asInt();
	req.speech_timeout     = str["speech_timeout"].asInt();
	req.silence_timeout    = str["silence_timeout"].asInt();
	req.noinput_timeout    = str["noinput_timeout"].asInt();

	return;
}

/*  ���ַ���ת���������ṹ�� */
void decode_str_2_check_req(Json::Value str, VOICE_CHECK_REQ &req)
{
	//Json::Value root_value = root["audio_detector_req"];

	string t_id = str["task_id"].asString();
	memcpy(req.task_id, t_id.c_str(), t_id.length() + 1);

	string url = str["media_url"].asString();
	memcpy(req.media_url, url.c_str(), url.length() + 1);

	req.start_time = str["start_time"].asInt();
	req.media_duration= str["duration"].asInt();
	req.value_post_time= str["value_post_time"].asInt();

	req.FS   = str["FS"].asInt();
	req.FFTL = str["FFTL"].asInt();
	req.STAL = str["STAL"].asInt();

	return;
}

/* ����������� string */
Json::Value decode_task_ctrl_str(zframe_t *id_frame, string str,void *req, E_BIZ_TYPE &req_type)
{
	Json::Reader reader;
	Json::Value root;
	Json::Value item;
	if (reader.parse(str, root))
	{
		//Json::Value root_value = root["request_type"];
		string root_value = root["request_type"].asString();
		item = root["request_value"];
		if (memcmp((char *)root_value.c_str(), "asr", strlen("asr")) == 0)
		{
			req_type = E_BIZ_TYPE_ASR;
		}
		else if (memcmp((char *)root_value.c_str(), "task_control", strlen("task_control")) == 0)
		{
			req_type = E_BIZ_TYPE_CTRL;
		}
		else if (memcmp((char *)root_value.c_str(), "audio_state_checker", strlen("audio_state_checker")) == 0)
		{
			req_type = E_BIZ_TYPE_CHECK;
		}
		else
		{
			//cout << "request_type[" << request_value << "] error!" << endl;
		}
	}
	return item;
}

/* �Ѵ�asr��������õ���Ϣ������ַ���
* "result":""
*/
string decode_data_from_asr(string str)
{
	int len = str.length();
	char *arr = new char[len]();
	memcpy(arr, str.c_str() + 1, len - 2);
	string temp_str(arr);
	Json::Reader reader;
	Json::Value root;
	string ret_str;
	if (reader.parse(temp_str, root))
	{
		ret_str = root["result"].asString();
	}
	delete[]arr;
	return ret_str;
}

string encode_task_ctrl_2_req( TASK_CTRL_INTER_RSP &rsp )
{
	Json::Value root; //root node

	root["task_id"] = rsp.task_id;
	root["result"] = rsp.result;

	string str = root.toStyledString();
	return str;
}

void send_rsp_2_task_ctrl(zframe_t *id_frame, void*sock, TASK_CTRL_INTER_RSP &rsp)
{
	string str = encode_task_ctrl_2_req(rsp);
	zframe_t *response_frame = zframe_new(str.c_str(), str.length());
	zframe_send(&id_frame, sock, ZFRAME_REUSE + ZFRAME_MORE);

	zframe_t *nul_frame = zframe_new("", 1);
	zframe_send(&nul_frame, sock, ZFRAME_REUSE + ZFRAME_MORE);
	zframe_send(&response_frame, sock, 0);

	return;
}

/*************************************************
Function:    noise_send_2_request
Description: �Ѵ���������Ϣ���͸�����ʶ������
Input:       AUDIO_DETECTOR_RSP rsp
			 zframe_t *id_frame  ���󷽴�������id
			 void *sock  �׽���
			 char *task_id ����Я���������ʶ
			 int msg_type  ��Ϣ����
Output:      null
Return:      void
Others:      ��������ҵ�����Ӧ����Ϣ�ṹ��һ������
ʹ�ô�һ���ӿ�
*************************************************/
void noise_send_2_request(zframe_t *id_frame, void *sock, VOICE_CHECK_RSP &rsp , const char* task_id, int msg_type)
{
	memcpy(rsp.task_id, task_id, strlen(task_id) + 1);
	if (msg_type != E_MSG_TYPE_BIZ_NORMAL)
	{
		rsp.result.Clip = 0.00;
		rsp.result.Speech = 0.00;
		rsp.result.Noise = 0.00;
	}

	switch (msg_type)
	{
	case E_MSG_TYPE_BIZ_NORMAL:
		break;
	case E_MSG_TYPE_ACCEPT_SUCC:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.msg, "recv msg success!", sizeof("recv msg success!"));
		rsp.status_code   = E_STATUS_ACCEPT_OK;
		break;
	case E_MSG_TYPE_BIZ_FAIL:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.msg, "connect rtsp server error!", sizeof("connect rtsp server error!"));
		rsp.status_code = E_STATUS_CONN_ERROR;
		break;
	case E_MSG_TYPE_JSON_ERROR:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.msg, "receive json msg error!", sizeof("receive json msg error!"));
		rsp.status_code = E_STATUS_JSON_ERROR;
		break;
	case E_MSG_TYPE_CONN_ERROR:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.msg, "cannot connect rtsp server!", sizeof("cannot connect rtsp server!"));
		rsp.status_code = E_STATUS_CONN_ERROR;
		break;
	case E_MSG_TYPE_DOG_ERROR:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		//memcpy(rsp.msg, "Encrypted dog authentication failur!", sizeof("Encrypted dog authentication failur!"));
		rsp.status_code = E_STATUS_AUTH_DOG_ERROR;
		break;
	case E_MSG_TYPE_USER_CANCEL:
		memcpy(rsp.msg, "user cancel success", sizeof("user cancel success"));
		rsp.status_code = E_STATUS_BIZ_CANCEL_SUCC;
		break;
	case E_MSG_TYPE_START_RTSP_ERROR:
		memcpy(rsp.msg, "start rtsp server error!", sizeof("start rtsp server error!"));
		rsp.status_code = E_STATUS_START_RTSP_ERR;
		break;
	case E_MSG_TYPE_SEND_FINISH:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.task_id, task_id, strlen(task_id) + 1);
		memcpy(rsp.msg, "send msg finish!", sizeof("send msg finish!"));
		rsp.status_code = E_STATUS_FINISH;
		break;
	case E_MSG_TYPE_STREAM_OVER:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.task_id, task_id, strlen(task_id) + 1);
		memcpy(rsp.msg, "send msg finish!", sizeof("send msg finish!"));
		rsp.status_code = E_STATUS_STREAM_IS_OVER;
		break;
	default:
		cout << "msg type error!" << endl;
	}

	string str = noise_rsp_2_str(rsp);
	zframe_t *response_frame = zframe_new(str.c_str(), str.length());
	zframe_send(&id_frame, sock, ZFRAME_REUSE + ZFRAME_MORE);

	zframe_t *nul_frame = zframe_new("", 1);
	zframe_send(&nul_frame, sock, ZFRAME_REUSE + ZFRAME_MORE);
	zframe_send(&response_frame, sock, 0);

	zframe_destroy(&nul_frame);
	zframe_destroy(&response_frame);

	return;
}

/*************************************************
Function:    noise_send_2_request
Description: �Ѵ���������Ϣ���͸�����ʶ������
Input:       AUDIO_DETECTOR_RSP rsp
zframe_t *id_frame  ���󷽴�������id
void *sock  �׽���
char *task_id ����Я���������ʶ
int msg_type  ��Ϣ����
Output:      null
Return:      void
Others:      ��������ҵ�����Ӧ����Ϣ�ṹ��һ������
ʹ�ô�һ���ӿ�
*************************************************/
void vad_send_2_request(zframe_t *id_frame, void *sock, AUDIO_DETECTOR_RSP &rsp, char* task_id, int msg_type)
{
	memcpy(rsp.task_id, task_id, strlen(task_id) + 1);
	switch (msg_type)
	{
	case E_MSG_TYPE_BIZ_NORMAL:
		break;
	case E_MSG_TYPE_ACCEPT_SUCC:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.msg, "recv msg success!", sizeof("recv msg success!"));
		rsp.status_code = E_STATUS_ACCEPT_OK;
		memcpy(rsp.result.text, "", sizeof(""));
		break;
	case E_MSG_TYPE_BIZ_FAIL:
	case E_MSG_TYPE_CONN_ERROR:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.msg, "connect rtsp server error!", sizeof("connect rtsp server error!"));
		rsp.status_code = E_STATUS_CONN_ERROR;
		memcpy(rsp.result.text, "", sizeof(""));
		break;
	case E_MSG_TYPE_START_RTSP_ERROR:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.msg, "start rtsp server error!", sizeof("start rtsp server error!"));
		rsp.status_code = E_STATUS_START_RTSP_ERR;
		memcpy(rsp.result.text, "", sizeof(""));
		break;
	case E_MSG_TYPE_DOG_ERROR:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		//memcpy(rsp.msg, "Encrypted dog authentication failur!", sizeof("Encrypted dog authentication failur!"));
		rsp.status_code = E_STATUS_AUTH_DOG_ERROR;
		break;
	case E_MSG_TYPE_SEND_FINISH:
		rsp.sequence = 0;
		rsp.result.start_time_stamp = rsp.result.end_time_stamp = 0; //�ɹ���ʧ��ʱ�䶼������
		memcpy(rsp.msg, "send msg finish!", sizeof("send msg finish!"));
		rsp.status_code = E_STATUS_FINISH;
		memcpy(rsp.result.text, "", sizeof(""));
		break;
	case E_MSG_TYPE_USER_CANCEL:
		memcpy(rsp.msg, "user cancel success", sizeof("user cancel success") );
		memcpy(rsp.result.text, "", sizeof(""));
		rsp.status_code = E_STATUS_BIZ_CANCEL_SUCC;
		break;
	default:
		cout << "msg type error!" << endl;
	}

	string str = detector_rsp_2_str(rsp);
	zframe_t *response_frame = zframe_new(str.c_str(), str.length());
	zframe_send(&id_frame, sock, ZFRAME_REUSE + ZFRAME_MORE);

	zframe_t *nul_frame = zframe_new("", 1);
	zframe_send(&nul_frame, sock, ZFRAME_REUSE + ZFRAME_MORE);
	zframe_send(&response_frame, sock, 0);

	zframe_destroy(&nul_frame);
	zframe_destroy(&response_frame);

	return;
}

void Bytes2Shorts(char* cBuf, int cBufLen, short* sBuf)
{
	char bLength = 2;
	int sLen = cBufLen / bLength;

	for (int i = 0; i < sLen; i++)
	{
		sBuf[i] = (short)(((cBuf[(i << 1) + 1] & 0xff) << 8) | (cBuf[i << 1] & 0xff));
	}
	return;
}

void Shorts2Floats(short* buf, int len, float *fBuf)
{
	float c = 1.0f / 32768.0f;
	for (int i = 0; i < len; i++)
	{
		fBuf[i] = (float)buf[i] * c;
	}
	return;
}

string noisereq2string(VOICE_CHECK_REQ *req)
{
	ostringstream os;
	os << req->task_id << "\t"
		<< req->media_url << "\t"
		<< req->start_time << "\t"
		<< req->media_duration << "\t"
		<< req->value_post_time;
	return os.str();
}

string audio_req_2_string(AUDIO_DETECTOR_REQ *req)
{
	ostringstream os;
	os << req->task_id << "\t"
		<< req->media_url << "\t"
		<< req->start_time << "\t"
		<< req->duration;
	return os.str();
}

string noisersp2string(VOICE_CHECK_RSP *rsp)
{
	ostringstream os;
	os << rsp->task_id << "\t"
		<< rsp->sequence << "\t"
		<< rsp->status_code << "\t"
		<< rsp->result.start_time_stamp << "\t"
		<< rsp->result.end_time_stamp << "\t"
		<< rsp->msg;
	return os.str();
}

string audio_rsp_2_string(AUDIO_DETECTOR_RSP*rsp)
{
	ostringstream os;
	os << rsp->task_id << "\t"
		<< rsp->sequence << "\t"
		<< rsp->status_code << "\t"
		<< rsp->result.start_time_stamp << "\t"
		<< rsp->result.end_time_stamp << "\t"
		<< rsp->msg;
	return os.str();
}

VOICE_CHECK_REQ string2noisereq(string str)
{
	VOICE_CHECK_REQ req;
	istringstream is(str);
	is >> req.task_id >> req.media_url >> req.start_time >> req.media_duration >> req.value_post_time;
	return req;
}

VOICE_CHECK_RSP string2noisersp(string str)
{
	VOICE_CHECK_RSP rsp;
	istringstream is(str);
	is >> rsp.task_id >> rsp.sequence >> rsp.status_code >> rsp.result.start_time_stamp >> rsp.result.end_time_stamp>>rsp.msg;
	return rsp;
}

AUDIO_DETECTOR_REQ string_2_audioreq(string str)
{
	AUDIO_DETECTOR_REQ req;
	istringstream is(str);
	is >> req.task_id >> req.media_url >> req.start_time >> req.duration;
	return req;
}

AUDIO_DETECTOR_RSP string_2_audiorsp(string str)
{
	AUDIO_DETECTOR_RSP rsp;
	istringstream is(str);
	is >> rsp.task_id >> rsp.sequence >> rsp.status_code >> rsp.result.start_time_stamp
		>> rsp.result.end_time_stamp >> rsp.msg;
	return rsp;
}
