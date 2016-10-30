#ifndef _MESSAGE_PARSER_H_
#define _MESSAGE_PARSER_H_

#include "PublicHead.h"
#include "json\json.h"
#include "libczmq\include\czmq.h"

//应答消息基类
class CResponseMsg {
public:
	string		task_id;
	
public:
	CResponseMsg(string strTaskID);
	virtual E_BIZ_TYPE	getRspMsgType();
	virtual void		sendResponse(zframe_t *id_frame, void *sock, string strCtrlResult){ }
	virtual void		sendResponse(zframe_t *id_frame, void *sock, E_MSG_TYPE msgType, bool bWorker = true){ }

	void				sendZframe(zframe_t *id_frame, void *sock, string strJson, bool bWorker = true);
};

//控制应答消息类
class CCtrlResponse: public CResponseMsg
{
public:
	CCtrlResponse(string strTaskID);
	virtual E_BIZ_TYPE	getRspMsgType();
	virtual void		sendResponse(zframe_t *id_frame, void *sock, string strCtrlResult);

};

//语音检测和识别消息基类
class CVoiceResponse : public CResponseMsg{
public:
	string      message;
	int         status_code;
	int         sequence;
	int			start_time_stamp; //开始的时间戳(以音视频内的时间戳为准)
	int			end_time_stamp;   //结束的时间戳(以音视频内的时间戳为准)
public:
	CVoiceResponse(string strTaskID);

	virtual E_BIZ_TYPE	getRspMsgType();
	virtual string		RspMsg2JsonStr(E_MSG_TYPE msgType);
	virtual void		sendResponse(zframe_t *id_frame, void *sock, E_MSG_TYPE msgType, bool bWorker = true);
	virtual void		setCheckParameter(float	fClip, float fSpeech, float fNoise) {}
	virtual void		setRecogParameter(string strText){}
	void				buildRspMsg(E_MSG_TYPE msgType);

};
//语音检测应答消息
class CCheckResponse :public CVoiceResponse {
public:
	float		Clip;
	float		Speech;
	float		Noise;

public:
	CCheckResponse(string strTaskID);
	virtual E_BIZ_TYPE	getRspMsgType();
	virtual string		RspMsg2JsonStr(E_MSG_TYPE msgType);
	virtual void		setCheckParameter(float	fClip, float fSpeech, float fNoise);
};
//语音识别应答消息
class CRecogResponse :public CVoiceResponse {
public:
	string		text;

public:
	CRecogResponse(string strTaskID);
	virtual E_BIZ_TYPE	getRspMsgType();
	virtual string		RspMsg2JsonStr(E_MSG_TYPE msgType);
	virtual void		setRecogParameter(string strText);
};

class CMessageParser
{
public:
	CMessageParser();
	~CMessageParser();

	bool Init(string strReqJson);
	void GetCheckRequest(VOICE_CHECK_REQ &req);
	void GetRecogRequest(AUDIO_DETECTOR_REQ &req);
	E_BIZ_TYPE GetReqMsgType();
	
	string GetReqTaskID();
	string GetValueFromKey(string strKey);
	void   BuildRspJson(CVoiceResponse *rsp, E_MSG_TYPE msgType);

	static string ASRMsgJson2Str(string str, string strKey);
	void GetCtrlReq(TASK_CONTROL_INTERFACE &task_req);
private:
	//string				m_strReqJson;		//请求消息
	Json::Value				m_valJsonReqValue;	//root["request_value"]的值

	E_BIZ_TYPE				m_emMsgType;		//消息类型：检测、识别、控制
	VOICE_CHECK_REQ			m_stCheckReqMsg;	//语音检测请求消息结构体
	AUDIO_DETECTOR_REQ		m_stRecogReqMsg;	//语音识别请求消息结构体

};


#endif // !_MESSAGE_PARSER_H_
