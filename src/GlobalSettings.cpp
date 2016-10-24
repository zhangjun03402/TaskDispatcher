#include <fstream>
#include "GlobalSettings.h"
#include "json\json.h"
#include "libczmq\include\czmq.h"

using namespace Json;

unordered_map<string, st_task_ctrl>	CGlobalSettings::mapTaskID2Ctrl;
CRITICAL_SECTION			CGlobalSettings::csMapCtrl;

CGlobalSettings::CGlobalSettings()
{
	m_strASRServerAddr = "tcp://192.168.2.167:5859";
}

CGlobalSettings::~CGlobalSettings()
{
}

CGlobalSettings& CGlobalSettings::GetInstance()
{
	static CGlobalSettings Instance;
	return Instance;
}

string CGlobalSettings::GetDetectBrokerBackAddr()
{
	return m_strDetectBrokerBackAddr;
}

string CGlobalSettings::GetRecogBrokerBackAddr()
{
	return m_strRecogBrokerBackAddr;
}

string CGlobalSettings::GetASRServerAddr()
{
	return m_strASRServerAddr;
}

void CGlobalSettings::Bytes2Shorts(char* cBuf, int cBufLen, short* sBuf)
{
	char bLength = 2;
	int sLen = cBufLen / bLength;

	for (int i = 0; i < sLen; i++)
	{
		sBuf[i] = (short)(((cBuf[(i << 1) + 1] & 0xff) << 8) | (cBuf[i << 1] & 0xff));
	}
	return;
}

void CGlobalSettings::Shorts2Floats(short* buf, int len, float *fBuf)
{
	float c = 1.0f / 32768.0f;
	for (int i = 0; i < len; i++)
	{
		fBuf[i] = (float)buf[i] * c;
	}
	return;
}

bool CGlobalSettings::InitGlobalParametr(string strConfigFile)
{
	std::ifstream fin;
	Json::Reader JsonReader;
	Json::Value  JsonText;

	fin.open(strConfigFile);
	if (!fin.is_open())
		return false;

	if (!JsonReader.parse(fin, JsonText, false))
		return false;

	m_strDetectBrokerBackAddr	= JsonText[DETECT_BROKER_BACK_ADDR].asString();
	m_strRecogBrokerBackAddr	= JsonText[RECOGNIZE_BROKER_BACK_ADDR].asString();
	m_strASRServerAddr			= JsonText[ASR_SERVER_ADDR].asString();

	return true;
}
bool CGlobalSettings::Initialize()
{
	string	strPath, strModulePath, strLog, strConfigFile;
	char	cPath[MAX_PATH];
	size_t	pos;

	::GetModuleFileName(NULL, (LPSTR)cPath, MAX_PATH);

	strPath = cPath;
	pos = strPath.find_last_of('\\', strPath.length());
	pos++;
	strModulePath = strPath.substr(0, pos);

	strConfigFile = strModulePath + CONFIG_FILE;
	if (_access(strConfigFile.c_str(), 0) != 0)
		return false;
	else 
	{
		strLog = strModulePath + LOG_PATH;
		if (_access(strLog.c_str(), 0) != 0)
		{
			if (CreateDirectory(strLog.c_str(), NULL) == -1)
				return false;
		}
	}
	if (!InitGlobalParametr(strConfigFile))
		return false;

	return true;
}