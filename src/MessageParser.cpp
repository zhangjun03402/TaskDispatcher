
#include "MessageParser.h"
#include <cassert>

//rep msg base class
CResponseMsg::CResponseMsg(string strTaskID)
{
	task_id = strTaskID;

}
E_BIZ_TYPE	CResponseMsg::getRspMsgType()
{
	return E_BIZ_TYPE_UNKOWN;
}
void CResponseMsg::sendZframe(zframe_t *id_frame, void *sock, string strJson, bool bWorker)
{
	zframe_t *response_frame = zframe_new(strJson.c_str(), strJson.size());

	zmsg_t *msg = zmsg_new();
	if(bWorker)
		zmsg_addmem(msg, NULL, 0);			// delimiter
	zmsg_append(msg, &id_frame);
	zmsg_addmem(msg, NULL, 0);			// delimiter
	zmsg_append(msg, &response_frame);
	
	zmsg_send(&msg, sock);
	assert(response_frame == NULL);
}

CCtrlResponse::CCtrlResponse(string strTaskID) : CResponseMsg(strTaskID)
{
}
E_BIZ_TYPE CCtrlResponse::getRspMsgType()
{
	return E_BIZ_TYPE_CTRL;
}
void CCtrlResponse::sendResponse(zframe_t *id_frame, void *sock, string strCtrlResult)
{
	Json::Value root;
	string strJson;

	root["task_id"]	= task_id;
	root["result"]	= strCtrlResult;

	strJson = root.toStyledString();

	sendZframe(id_frame, sock, strJson, false);
}

//voice response base class of check and recog
CVoiceResponse::CVoiceResponse(string strTaskID):CResponseMsg(strTaskID)
{
	status_code			= 0;
	sequence			= 0;
	start_time_stamp	= 0;
	end_time_stamp		= 0;
}
E_BIZ_TYPE CVoiceResponse::getRspMsgType()
{
	return E_BIZ_TYPE_UNKOWN;
}
void CVoiceResponse::sendResponse(zframe_t *id_frame, void *sock, E_MSG_TYPE msgType, bool bWorker)
{
	string strJson = this->RspMsg2JsonStr(msgType);

	sendZframe(id_frame, sock, strJson, bWorker);
}
string	CVoiceResponse::RspMsg2JsonStr(E_MSG_TYPE msgType)
{
	return string();
}
//根据响应消息类型，生成返回消息
void CVoiceResponse::buildRspMsg(E_MSG_TYPE msgType)
{
	switch (msgType)
	{
	case E_MSG_TYPE_BIZ_NORMAL:
		message = "Normal data!";
		status_code = E_STATUS_BIZ_NORMAL;
		break;
	case E_MSG_TYPE_ACCEPT_SUCC:
		message = "Recv msg success!";
		status_code = E_STATUS_ACCEPT_OK;
		break;
	case E_MSG_TYPE_BIZ_FAIL:
		message = "Service not available!";
		status_code = E_STATUS_ISNULL;
		break;
	case E_MSG_TYPE_CONN_ERROR:
		message = "Connect rtsp server error!";
		status_code = E_STATUS_CONN_ERROR;
		break;
	case E_MSG_TYPE_START_RTSP_ERROR:
		message = "Start(Restart) rtsp server error!";
		status_code = E_STATUS_START_RTSP_ERR;
		break;
	case E_MSG_TYPE_DOG_ERROR:
		message	= "Encrypted dog authentication failed!";
		status_code = E_STATUS_AUTH_DOG_ERROR;
		break;
	case E_MSG_TYPE_JSON_ERROR:
		message = "receive json msg error!";
		status_code = E_STATUS_JSON_ERROR;
		break;
	case E_MSG_TYPE_SEND_FINISH:
		message = "send message finish!";
		status_code = E_STATUS_FINISH;
		break;
	case E_MSG_TYPE_STREAM_OVER:
		message = "stream is over!";
		status_code = E_STATUS_STREAM_IS_OVER;
		break;
	case E_MSG_TYPE_USER_CANCEL:
		message = "user cancel success!";
		status_code = E_STATUS_BIZ_CANCEL_SUCC;
		break;
	case E_MSG_TYPE_SERVER_BUSY:
		message = "server is busy，please try again later!";
		status_code = E_STATUS_FINISH;
		break;
	default:
		break;
	}
}

//check rsp msg class
CCheckResponse::CCheckResponse(string strTaskID): CVoiceResponse(strTaskID)
{
	Clip	= 0.0;
	Speech	= 0.0;
	Noise	= 0.0;
}
E_BIZ_TYPE	CCheckResponse::getRspMsgType()
{
	return E_BIZ_TYPE_CHECK;
}

void CCheckResponse::setCheckParameter(float fClip, float fSpeech, float fNoise)
{
	Clip	= fClip;
	Speech	= fSpeech;
	Noise	= fNoise;
}
string	CCheckResponse::RspMsg2JsonStr(E_MSG_TYPE msgType)
{
	buildRspMsg(msgType);	//生成message和status_code字段的值

	Json::Value root; //root node
	root["task_id"]		= task_id;
	root["message"]		= message;
	root["sequence"]	= sequence;
	root["status_code"] = status_code;

	Json::Value result;
	result["start_time"]= start_time_stamp;
	result["end_time"]	= end_time_stamp;
	result["Clip"]		= Clip;
	result["Speech"]	= Speech;
	result["Noise"]		= Noise;

	root["result"]		= result;

	return root.toStyledString();
}

//recog rsp msg class
CRecogResponse::CRecogResponse(string strTaskID) : CVoiceResponse( strTaskID )
{
}
E_BIZ_TYPE	CRecogResponse::getRspMsgType()
{
	return E_BIZ_TYPE_ASR;
}

void CRecogResponse::setRecogParameter(string strText)
{
	text = strText;
}

string	CRecogResponse::RspMsg2JsonStr(E_MSG_TYPE msgType)
{
	buildRspMsg(msgType);	//生成message和status_code字段的值

	Json::Value root; //root node
	root["task_id"]		= task_id;
	root["message"]		= message;
	root["sequence"]	= sequence;
	root["status_code"]	= status_code;

	Json::Value result;
	result["start_time"]= start_time_stamp;
	result["end_time"]	= end_time_stamp;
	result["text"]		= text;

	root["result"]		= result;

	return root.toStyledString();
}


CMessageParser::CMessageParser( )
{
	m_emMsgType = E_BIZ_TYPE_UNKOWN;
}

CMessageParser::~CMessageParser()
{}

bool CMessageParser::Init(string strReqJson)
{
	Json::Reader	reader;
	Json::Value		root;
	if (!reader.parse(strReqJson, root))
		return false;
	m_valJsonReqValue = root["request_value"];
	string root_value = root["request_type"].asString();
	if (root_value == "asr")
		m_emMsgType = E_BIZ_TYPE_ASR;
	else if (root_value == "task_control")
		m_emMsgType = E_BIZ_TYPE_CTRL;
	else if (root_value == "audio_state_checker")
		m_emMsgType = E_BIZ_TYPE_CHECK;
	else {
		m_emMsgType = E_BIZ_TYPE_UNKOWN;
		return false;
	}
		
	return true;
}

E_BIZ_TYPE CMessageParser::GetReqMsgType()
{
	return m_emMsgType;
}

string CMessageParser::GetValueFromKey(string strKey)
{
	return m_valJsonReqValue[strKey].asString();
}

string CMessageParser::GetReqTaskID()
{
	return GetValueFromKey("task_id");
}

void CMessageParser::GetCheckRequest(VOICE_CHECK_REQ &req)
{
	assert(m_emMsgType == E_BIZ_TYPE_CHECK);

	string t_id = m_valJsonReqValue["task_id"].asString();
	memcpy(req.task_id, t_id.c_str(), t_id.length() + 1);

	string url = m_valJsonReqValue["media_url"].asString();
	memcpy(req.media_url, url.c_str(), url.length() + 1);

	req.start_time		= m_valJsonReqValue["start_time"].asInt();
	req.media_duration	= m_valJsonReqValue["duration"].asInt();
	req.value_post_time = m_valJsonReqValue["value_post_time"].asInt();

	req.FS		= m_valJsonReqValue["FS"].asInt();
	req.FFTL	= m_valJsonReqValue["FFTL"].asInt();
	req.STAL	= m_valJsonReqValue["STAL"].asInt();
}

void CMessageParser::GetRecogRequest(AUDIO_DETECTOR_REQ &req)
{
	assert(m_emMsgType == E_BIZ_TYPE_ASR);

	string t_id = m_valJsonReqValue["task_id"].asString();
	memcpy(req.task_id, t_id.c_str(), t_id.length() + 1);

	string url = m_valJsonReqValue["media_url"].asString();
	memcpy(req.media_url, url.c_str(), url.length() + 1);

	req.start_time			= m_valJsonReqValue["start_time"].asInt();
	req.duration			= m_valJsonReqValue["duration"].asInt();
	req.vad_state_duration	= m_valJsonReqValue["vad_state_duration"].asInt();

	req.reconn_num			= m_valJsonReqValue["reconn_num"].asInt();
	req.reconn_interval		= m_valJsonReqValue["reconn_interval"].asInt();
	//string file_path = m_valJsonRoot["pcm_file_path"].asString();
	//memcpy(req.file_path, file_path.c_str(), file_path.length() + 1);

	req.level_threshold	= m_valJsonReqValue["level_threshold"].asInt();
	req.level_current	= m_valJsonReqValue["level_current"].asInt();
	req.speech_timeout	= m_valJsonReqValue["speech_timeout"].asInt();
	req.silence_timeout = m_valJsonReqValue["silence_timeout"].asInt();
	req.noinput_timeout = m_valJsonReqValue["noinput_timeout"].asInt();
}

void CMessageParser::BuildRspJson(CVoiceResponse *rsp, E_MSG_TYPE msgType)
{
	E_BIZ_TYPE emType = rsp->getRspMsgType();

	switch (msgType)
	{
	case E_MSG_TYPE_BIZ_NORMAL:
		break;
	case E_MSG_TYPE_ACCEPT_SUCC:
		rsp->message			= "recv msg success!";
		rsp->status_code		= E_STATUS_ACCEPT_OK;
		break;
	case E_MSG_TYPE_BIZ_FAIL:
	case E_MSG_TYPE_CONN_ERROR:
		rsp->message			= "connect rtsp server error!";
		rsp->status_code		= E_STATUS_CONN_ERROR;
		break;
	case E_MSG_TYPE_START_RTSP_ERROR:
		rsp->message			= "start rtsp server error!";
		rsp->status_code		= E_STATUS_START_RTSP_ERR;
		break;
	case E_MSG_TYPE_DOG_ERROR:
		//rsp->message			= "Encrypted dog authentication failur!";
		rsp->status_code		= E_STATUS_AUTH_DOG_ERROR;
		break;
	case E_MSG_TYPE_SEND_FINISH:
		rsp->message			= "send message finish!";
		rsp->status_code		= E_STATUS_FINISH;
		break;
	case E_MSG_TYPE_STREAM_OVER:
		rsp->message			= "stream is over!";
		rsp->status_code		= E_STATUS_STREAM_IS_OVER;
	case E_MSG_TYPE_USER_CANCEL:
		rsp->message			= "user cancel success";
		rsp->status_code		= E_STATUS_BIZ_CANCEL_SUCC;
		break;
	default:
		break;
	}

}
/*
将如下json串： [{"result":	"[公司出了一千多]"}]
转换为：		{"result":	"[公司出了一千多]"}
根据strKey="result",返回："公司出了一千多"
*/
string CMessageParser::ASRMsgJson2Str(string strJson, string strKey)
{
	string retStr;
	string s = strJson.substr(1, strJson.size() - 2);

	Json::Reader reader;
	Json::Value root;
	if (reader.parse(s, root))
	{
		//retStr = root["result"].asString();
		retStr = root[strKey].asString();
	}
	return retStr;
}

void CMessageParser::GetCtrlReq(TASK_CONTROL_INTERFACE &task_req)
{
	assert(m_emMsgType == E_BIZ_TYPE_CTRL);

	string t_id = m_valJsonReqValue["task_id"].asString();
	memcpy(task_req.task_id, t_id.c_str(), t_id.length() + 1);
	string flags = m_valJsonReqValue["ctrl_flags"].asString();
	memcpy(task_req.ctrl_flag, flags.c_str(), flags.length() + 1);
}
