/****************************************************************
* Copyright: 2016, Ling-Ban Tech. Co., Ltd.
* File name: PublicHead;
* Description: 公共头文件，主要存放一些公共需要的数据
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
const int URL_LEN       = 256; //url长度
const int TEXT_LEN      = 1024; //返回的text文本的长度
const int RSP_MSG_LEN   = 256; //响应消息长度
const int Max_Threads   = 5;   //最大线程数
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
	LOG_ERR  = 0x00,   //错误 级别最高，默认输出  
	LOG_WARN = 0x01,   //警告  
	LOG_INFO = 0x02,   //提示  
	LOG_TRC  = 0x03,   //一般  
	LOG_DBG  = 0x04,   //调试  
};
#endif

//返回码/状态码枚举，数据以10进制表示，后期有需要则以16进制表示
typedef enum _E_STATUS_CODE_
{

	//以2开关的是成功类的信息
	E_STATUS_ACCEPT_OK = 200, //表示接收成功
	E_STATUS_BIZ_NORMAL = 201,
	E_STATUS_STREAM_IS_OVER = 297, //流播放结束
	E_STATUS_BIZ_CANCEL_SUCC = 298, //取消成功
	E_STATUS_FINISH = 299, //表示处理完毕

	//以3开关的都是异常类的错误
	E_STATUS_ISNULL = 300, //数据流为空
	E_STATUS_CONN_ERROR = 301, //连接rtsp服务器失败
	E_STATUS_NOT_EXISTS = 302, //rtsp连接不到数据
	E_STATUS_RECONN_FAIL = 303, //重连失败
	E_STATUS_JSON_ERROR = 304, //发送的json错误
	E_STATUS_AUTH_DOG_ERROR = 305, //加密狗验证错误
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
	E_MSG_TYPE_BIZ_NORMAL  = 1, //正常业务类消息
	E_MSG_TYPE_ACCEPT_SUCC = 2, //连接成功
	E_MSG_TYPE_SEND_FINISH = 3,
	E_MSG_TYPE_BIZ_FAIL    = 4, //
	E_MSG_TYPE_JSON_ERROR  = 5,  //json错误
	E_MSG_TYPE_CONN_ERROR  = 6,   //连接错误，连接不到rtsp
	E_MSG_TYPE_DOG_ERROR   = 7,    //加密狗验证错误
	E_MSG_TYPE_USER_CANCEL = 8,
	E_MSG_TYPE_STREAM_OVER = 9,  //流播放结束
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

/*  控制任务类型 */
typedef enum _E_TASK_CONTROL_
{
	E_CTRL_NONE = -1,	//不进行控制
	E_CTRL_PAUSE  = 0,	//暂停
	E_CTRL_RESUME = 1,	//恢复
	E_CTRL_CANCEL = 2,	//取消
	E_CTRL_SUCC   = 3	//成功
}E_TASK_CONTROL;

typedef struct _ST_TASK_CTRL_
{
	int				thread_no; //进程号，非进程id
	E_TASK_CONTROL	ctrl_type; //控制类型
}st_task_ctrl;

typedef enum
{
	E_PLAY_LAST = 0, //播放标志未结束
	E_PLAY_OVER = 1  //播放标志结束
}E_PLAY_STATUS;

/* 连接模式 */
typedef enum
{
	E_RTSP_CONN_MODE_UDP = 0,
	E_RTSP_CONN_MODE_TCP = 1
}E_Conn_Mode;

typedef struct _S_VAD_RESULT_
{
	int   start_time_stamp; //开始的时间戳(以音视频内的时间戳为准)
	int   end_time_stamp;   //结束的时间戳(以音视频内的时间戳为准)
	char  text[TEXT_LEN];
}S_VAD_RESULT;

typedef struct _S_NOISE_RESULT_
{
	int start_time_stamp; //开始的时间戳(以音视频内的时间戳为准)
	int end_time_stamp;   //结束的时间戳(以音视频内的时间戳为准)
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

/* 语音识别请求结构体 */
typedef struct _AUDIO_DETECTOR_REQ_
{
	char task_id[TASK_ID_LEN];             //任务id
	char media_url[URL_LEN];  //音视频流地址
	UINT start_time;          //音频开始时间,如20分钟，则20分钟后的数据才开始处理(单位毫秒)
	int  duration;            //音频识别时长，如1小时，则只识别一小时的数据
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


/* 语音识别应答结构体 */
typedef struct _AUDIO_DETECTOR_RSP_
{
	char         task_id[TASK_ID_LEN];       //任务id
	char         msg[RSP_MSG_LEN]; //响应消息
	int          sequence;      //识别结果序列
	int          status_code;   //识别状态码
	S_VAD_RESULT result;        //识别状态码描述，识别结果
}AUDIO_DETECTOR_RSP;

/* 语音检测请求结构体 */
typedef struct _VOICE_CHECK_REQ_
{
	char task_id[TASK_ID_LEN];  //任务id
	char media_url[URL_LEN];   //音视频流地址
	UINT  start_time;          //音频处理开始时间,如20分钟，则表示20分钟后的数据才开始处理或通过协议直接请求20分钟后的数据，(毫秒)
	int  media_duration;       //音频处理时长，如1小时，则只检测一个小时的数据
	int  value_post_time;      //结果推送间隔，即以此时间间隔推送结果 (此时应该判断是否音视频流已处理完毕，如果处理完毕，也推送)
	int  FS;
	int  FFTL;
	int  STAL;
	int  reconn_num;
	int  reconn_interval;
	char file_path[PCM_FILE_LEN];
}VOICE_CHECK_REQ;

/* 语音检测应答结构体 */
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
