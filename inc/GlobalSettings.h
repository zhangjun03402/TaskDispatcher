#pragma once
#include <string>
#include <unordered_map>
#include "PublicHead.h"
#include "libczmq\include\czmq.h"

#ifdef WIN32
#include <Windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#define  CRITICAL_SECTION   pthread_mutex_t
#define  _vsnprintf         vsnprintf
#endif

using std::string;
using std::unordered_map;

class CGlobalSettings
{
private:
	CGlobalSettings();
	CGlobalSettings(const CGlobalSettings &) {}
	CGlobalSettings & operator = (const CGlobalSettings &) {}
public:
	~CGlobalSettings();

	static CGlobalSettings& GetInstance();

	bool Initialize();
	bool InitGlobalParametr(string strConfigFile);
	string GetDetectBrokerBackAddr();
	string GetRecogBrokerBackAddr();
	string GetASRServerAddr();

public:
	//全局的变量
	static unordered_map<string, st_task_ctrl>	mapTaskID2Ctrl;	//控制消息map全局变量
	static CRITICAL_SECTION				csMapCtrl;

	//全局功能函数
	static void Bytes2Shorts(char* cBuf, int cBufLen, short* sBuf);
	static void Shorts2Floats(short* buf, int len, float *fBuf);

private:
	
	string m_strASRServerAddr;
	string m_strDetectBrokerBackAddr;
	string m_strRecogBrokerBackAddr;

};
