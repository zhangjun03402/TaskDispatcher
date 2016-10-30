/****************************************************************
* Copyright: 2016, Ling-Ban Tech. Co., Ltd.
* File name: PublicHead;
* Description: ����ͷ�ļ�����Ҫ���һЩ������Ҫ������
*
* Author: sundongkai
* Version: V0.1
* Date:
* History:
******************************************************************/
#pragma once

#include <iostream>
#include <string>
#include "MacrosDefine.h"

using namespace std;

typedef unsigned int  UINT;
typedef unsigned char UCHAR;

const int TASK_ID_LEN   = 64;
const int URL_LEN       = 256; //url����
const int TEXT_LEN      = 1024; //���ص�text�ı��ĳ���
const int RSP_MSG_LEN   = 256; //��Ӧ��Ϣ����
const int Max_Threads   = 5;   //����߳���
const int RTSP_DATA_LEN = 800;
const int MSG_BODY_LEN  = 1024;
const int BUF_SIZE      = 1024;
const int TASK_CTRL_LEN = 32;
const int RESULT_LEN    = 10;
const int PCM_FILE_LEN  = 64;
const int FILE_NAME_LEN = 256;

//const float M_PI = 3.14159265358979323846;

//unsigned int UInitTime = 0;
#if 0
enum _Log_Level_
{
	LOG_ERR  = 0x00,   //���� ������ߣ�Ĭ�����  
	LOG_WARN = 0x01,   //����  
	LOG_INFO = 0x02,   //��ʾ  
	LOG_TRC  = 0x03,   //һ��  
	LOG_DBG  = 0x04,   //����  
};
#endif

//������/״̬��ö�٣�������10���Ʊ�ʾ����������Ҫ����16���Ʊ�ʾ
typedef enum _E_STATUS_CODE_
{

	//��2���ص��ǳɹ������Ϣ
	E_STATUS_ACCEPT_OK = 200, //��ʾ���ճɹ�
	E_STATUS_BIZ_NORMAL = 201,
	E_STATUS_STREAM_IS_OVER = 297, //�����Ž���
	E_STATUS_BIZ_CANCEL_SUCC = 298, //ȡ���ɹ�
	E_STATUS_FINISH = 299, //��ʾ�������

	//��3���صĶ����쳣��Ĵ���
	E_STATUS_ISNULL = 300, //������Ϊ��
	E_STATUS_CONN_ERROR = 301, //����rtsp������ʧ��
	E_STATUS_NOT_EXISTS = 302, //rtsp���Ӳ�������
	E_STATUS_RECONN_FAIL = 303, //����ʧ��
	E_STATUS_JSON_ERROR = 304, //���͵�json����
	E_STATUS_AUTH_DOG_ERROR = 305, //���ܹ���֤����
	E_STATUS_START_RTSP_ERR = 306,

}E_STATUS_CODE;

typedef enum _E_RTSP_STATUS_CODE_
{
	E_RTSP_STATUS_CONN_SUCC   = 0,
	E_RTSP_STATUS_CONN_FAIL   = 1,
	E_RTSP_STATUS_NO_DATA     = 2,
	E_RTSP_STATUS_START_RECON = 3,
	E_RTSP_STATUS_RECON_SUCC  = 4,
	E_RTSP_STATUS_RECON_FAIL  = 5
}E_RTSP_STATUS_CODE;

typedef enum _E_BIZ_NAME_
{
	E_BIZ_NAME_CHECK = 0,
	E_BIZ_NAME_ASR   = 1
}E_BIZ_NAME;

typedef enum _E_MSG_TYPE_
{
	E_MSG_TYPE_BIZ_NORMAL  = 1, //����ҵ������Ϣ
	E_MSG_TYPE_ACCEPT_SUCC = 2, //���ӳɹ�
	E_MSG_TYPE_SEND_FINISH = 3,
	E_MSG_TYPE_BIZ_FAIL    = 4, //
	E_MSG_TYPE_JSON_ERROR  = 5,  //json����
	E_MSG_TYPE_CONN_ERROR  = 6,   //���Ӵ������Ӳ���rtsp
	E_MSG_TYPE_DOG_ERROR   = 7,    //���ܹ���֤����
	E_MSG_TYPE_USER_CANCEL = 8,
	E_MSG_TYPE_STREAM_OVER = 9,  //�����Ž���
	E_MSG_TYPE_START_RTSP_ERROR = 10
}E_MSG_TYPE;

typedef struct _log_st
{
	char path[128];
	int  fd;
	int  size;
	int  level;
	int  num;
}st_log;

typedef enum _E_BIZ_TYPE_
{
	E_BIZ_TYPE_UNKOWN	= -1,
	E_BIZ_TYPE_CTRL		= 0,
	E_BIZ_TYPE_ASR		= 1,
	E_BIZ_TYPE_CHECK	= 2
}E_BIZ_TYPE;

/*  ������������ */
typedef enum _E_TASK_CONTROL_
{
	E_CTRL_NONE = -1,	//�����п���
	E_CTRL_PAUSE  = 0,	//��ͣ
	E_CTRL_RESUME = 1,	//�ָ�
	E_CTRL_CANCEL = 2,	//ȡ��
	E_CTRL_SUCC   = 3	//�ɹ�
}E_TASK_CONTROL;

typedef struct _ST_TASK_CTRL_
{
	int				thread_no; //���̺ţ��ǽ���id
	E_TASK_CONTROL	ctrl_type; //��������
}st_task_ctrl;

typedef enum
{
	E_PLAY_LAST = 0, //���ű�־δ����
	E_PLAY_OVER = 1  //���ű�־����
}E_PLAY_STATUS;

/* ����ģʽ */
typedef enum
{
	E_RTSP_CONN_MODE_UDP = 0,
	E_RTSP_CONN_MODE_TCP = 1
}E_Conn_Mode;

typedef struct _S_VAD_RESULT_
{
	int   start_time_stamp; //��ʼ��ʱ���(������Ƶ�ڵ�ʱ���Ϊ׼)
	int   end_time_stamp;   //������ʱ���(������Ƶ�ڵ�ʱ���Ϊ׼)
	char  text[TEXT_LEN];
}S_VAD_RESULT;

typedef struct _S_NOISE_RESULT_
{
	int start_time_stamp; //��ʼ��ʱ���(������Ƶ�ڵ�ʱ���Ϊ׼)
	int end_time_stamp;   //������ʱ���(������Ƶ�ڵ�ʱ���Ϊ׼)
	float Clip;
	float Speech;
	float Noise;
}S_NOISE_RESULT;

typedef struct _st_msg_queue_
{
	int msg_id;
	int msg_type;
	int end_flag;
	unsigned int msg_time;
	int msg_len;
	char *msg_body;
}st_msg_queue;

typedef struct _CHWAVEFORMATEX_
{
	unsigned short    wFormatTag;
	unsigned short    nChannels;
	unsigned int      nSamplesPerSec;
	unsigned int      nAvgBytesPerSec;
	unsigned short    nBlockAlign;
	unsigned short    wBitsPerSample;
	unsigned short    cbSize;
} CHWAVEFORMATEX;

/* ����ʶ������ṹ�� */
typedef struct _AUDIO_DETECTOR_REQ_
{
	char task_id[TASK_ID_LEN];             //����id
	char media_url[URL_LEN];  //����Ƶ����ַ
	UINT start_time;          //��Ƶ��ʼʱ��,��20���ӣ���20���Ӻ�����ݲſ�ʼ����(��λ����)
	int  duration;            //��Ƶʶ��ʱ������1Сʱ����ֻʶ��һСʱ������
	int  vad_state_duration;
	int  level_threshold;
	int  level_current;
	int  speech_timeout;
	int  silence_timeout;
	int  noinput_timeout;
	int  reconn_num;
	int  reconn_interval;
	char file_path[PCM_FILE_LEN];
}AUDIO_DETECTOR_REQ;


/* ����ʶ��Ӧ��ṹ�� */
typedef struct _AUDIO_DETECTOR_RSP_
{
	char         task_id[TASK_ID_LEN];       //����id
	char         msg[RSP_MSG_LEN]; //��Ӧ��Ϣ
	int          sequence;      //ʶ��������
	int          status_code;   //ʶ��״̬��
	S_VAD_RESULT result;        //ʶ��״̬��������ʶ����
}AUDIO_DETECTOR_RSP;

/* �����������ṹ�� */
typedef struct _VOICE_CHECK_REQ_
{
	char task_id[TASK_ID_LEN];  //����id
	char media_url[URL_LEN];   //����Ƶ����ַ
	UINT  start_time;          //��Ƶ����ʼʱ��,��20���ӣ����ʾ20���Ӻ�����ݲſ�ʼ�����ͨ��Э��ֱ������20���Ӻ�����ݣ�(����)
	int  media_duration;       //��Ƶ����ʱ������1Сʱ����ֻ���һ��Сʱ������
	int  value_post_time;      //������ͼ�������Դ�ʱ�������ͽ�� (��ʱӦ���ж��Ƿ�����Ƶ���Ѵ�����ϣ����������ϣ�Ҳ����)
	int  FS;
	int  FFTL;
	int  STAL;
	int  reconn_num;
	int  reconn_interval;
	char file_path[PCM_FILE_LEN];
}VOICE_CHECK_REQ;

/* �������Ӧ��ṹ�� */
typedef struct _VOICE_CHECK_RSP_
{
	char           task_id[TASK_ID_LEN];
	char           msg[RSP_MSG_LEN];
	int            status_code;
	int            sequence;
	S_NOISE_RESULT result;
}VOICE_CHECK_RSP;

typedef struct _TASK_CONTROL_INTERFACE_
{
	char task_id[TASK_ID_LEN];
	char ctrl_flag[TASK_CTRL_LEN]; /* 0:pause,1:restore,2:cancel */
}TASK_CONTROL_INTERFACE;

typedef struct _TASK_CTRL_INTER_RSP_
{
	char    task_id[TASK_ID_LEN];
	char    result[RESULT_LEN];
}TASK_CTRL_INTER_RSP;
