#ifndef _MESSAGE_PARSER_H_
#define _MESSAGE_PARSER_H_

#include "PublicHead.h"
#include "json\json.h"
#include "libczmq\include\czmq.h"

//Ӧ����Ϣ����
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

//����Ӧ����Ϣ��
class CCtrlResponse: public CResponseMsg
{
public:
	CCtrlResponse(string strTaskID);
	virtual E_BIZ_TYPE	getRspMsgType();
	virtual void		sendResponse(zframe_t *id_frame, void *sock, string strCtrlResult);

};

//��������ʶ����Ϣ����
class CVoiceResponse : public CResponseMsg{
public:
	string      message;
	int         status_code;
	int         sequence;
	int			start_time_stamp; //��ʼ��ʱ���(������Ƶ�ڵ�ʱ���Ϊ׼)
	int			end_time_stamp;   //������ʱ���(������Ƶ�ڵ�ʱ���Ϊ׼)
public:
	CVoiceResponse(string strTaskID);

	virtual E_BIZ_TYPE	getRspMsgType();
	virtual string		RspMsg2JsonStr(E_MSG_TYPE msgType);
	virtual void		sendResponse(zframe_t *id_frame, void *sock, E_MSG_TYPE msgType, bool bWorker = true);
	virtual void		setCheckParameter(float	fClip, float fSpeech, float fNoise) {}
	virtual void		setRecogParameter(string strText){}
	void				buildRspMsg(E_MSG_TYPE msgType);

};
//�������Ӧ����Ϣ
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
//����ʶ��Ӧ����Ϣ
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
	//string				m_strReqJson;		//������Ϣ
	Json::Value				m_valJsonReqValue;	//root["request_value"]��ֵ

	E_BIZ_TYPE				m_emMsgType;		//��Ϣ���ͣ���⡢ʶ�𡢿���
	VOICE_CHECK_REQ			m_stCheckReqMsg;	//�������������Ϣ�ṹ��
	AUDIO_DETECTOR_REQ		m_stRecogReqMsg;	//����ʶ��������Ϣ�ṹ��

};


#endif // !_MESSAGE_PARSER_H_
