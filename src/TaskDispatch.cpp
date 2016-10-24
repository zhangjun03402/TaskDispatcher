#include "..\inc\TaskDispatch.h"
#include "..\inc\GetRtspUrlData.h"
#include "..\inc\zhelpers.h"
#include "libczmq\include\czmq.h"
#include "..\inc\ChRTSP.h"
#include "CommonFunc.h"
#include "lic.h"
#include "MessageParser.h"
#include <glog\logging.h>
#include "GlobalSettings.h"
#include <iostream>
#include <string>
using namespace std ;
#pragma comment(lib, "Ws2_32.lib")

//using namespace Lingban::Common;
using namespace Lingban::CoreLib;

extern std::mutex			g_mtxCheckMsgQ[Max_Threads];  //语音质量检测临界区
extern std::mutex			g_mtxMapCtrl;
extern condition_variable	g_ConVar;
/*************************************************
Function:    ProcessNoiseCheck
Description: 噪音检测处理
Input:       void *context 创建的zmq消息上下文
             string:转换后的json串
		     const char *addr 要连接的目的资源地址，形式
	  如:"tcp://ip:port"
			 void *args  
Output: 
Return:      void
Others:      
*************************************************/
void ProcessNoiseCheck(void *context, string addr,void *args )
{
	assert(!addr.empty());

	int				iRet(-1);
	bool			bRet(false);
	string			strTaskID;
	E_BIZ_TYPE		emReqType(E_BIZ_TYPE_UNKOWN);

	int thread_no = (int)args;
	int thread_id = GetCurrentThreadId();

	LOG(INFO)<<"Enter "<<__FUNCTION__<<" process....!addr:["<<addr<<"],args:["<<args<<"],thread["<<GetCurrentThreadId()<<"]";

	void *sock = zmq_socket(context, ZMQ_DEALER);
	if (nullptr == sock)
	{
		LOG( INFO )<<"noise check module:create recv connect socket error:"<< zmq_strerror(zmq_errno());
		return;
	}

#if (defined (WIN32))
	s_set_id(sock,(intptr_t)args);
#else
	s_set_id(sock);          //  Set a printable identity
#endif

	cout << "voice check start success! current thread no is[" << thread_no << "],thread_id:"<<thread_id << endl;
	LOG(INFO) << "current thread number:" << (int)args << " thread_id:" << thread_id;

	//int r = zmq_connect(sock, addr);
	iRet = zmq_connect(sock, addr.c_str());
	if (iRet == -1)
	{
		LOG( INFO)<<"zmq_connect error: " << zmq_strerror(zmq_errno()) ;
		zmq_close(sock);
		return;
	}

	unique_ptr<GetRtspUrlData> getRtsp(new GetRtspUrlData);

	while (true)
	{
		zmsg_t *msg = zmsg_recv(sock);
		if (!msg) {
			LOG(INFO)<<"Worker[ "<<thread_no<<" ]:Failed received request message";
			return;
		}

		//保存源地址id帧和数据帧
		zframe_t *id_frame = zmsg_pop( msg );      //弹出帧头
		zframe_t *null_frame = zmsg_pop(msg);      //弹出空帧
		zframe_t *content_frame = zmsg_pop( msg ); //弹出数据帧

		char *content_str = zframe_strdup(content_frame);

		zmsg_destroy(&msg);
		zframe_destroy(&null_frame);
		zframe_destroy(&content_frame);

		//int  con_len		= zframe_size(content_frame);
		//char *id_str		= zframe_strhex(id_frame);

		LOG( INFO)<<"Check thread received request from the "<<thread_no<<"'s client whose thread:"<<thread_id<<" data["<<content_str<<"]" ;

		CMessageParser JsonParser;
		unique_ptr<CResponseMsg> upRspMessage;

		//decode_task_ctrl_str();
		//str_2_check_req(content_str, req); //把发送过来的请求数据转成结构体

		//EnterCriticalSection(&g_csCheck[thread_no]);
		//item = decode_task_ctrl_str(id_frame, content_str, (void*)&req, emReqType);
		//LeaveCriticalSection(&g_csCheck[thread_no]);

		JsonParser.Init(content_str);

		emReqType = JsonParser.GetReqMsgType();
		strTaskID = JsonParser.GetReqTaskID();

		/* 如果请求类型为任务取消，则进行任务取消处理程序，处理完毕后进入下一轮循环 */
		if (E_BIZ_TYPE_CTRL == emReqType)
		{
			LOG(INFO) << "ProcessNoiseCheck req_type E_BIZ_TYPE_CTRL";

			upRspMessage = unique_ptr<CCtrlResponse>(new CCtrlResponse(strTaskID));

			TASK_CONTROL_INTERFACE task_req;
			st_task_ctrl st;
			st.thread_no = thread_no;

			//thread t([getRtsp] {getRtsp->UserCancelStream();});
			//decode_str_2_control_req(id_frame, item, task_req);

			JsonParser.GetCtrlReq(task_req);
			if (stricmp(task_req.ctrl_flag, "cancel") == 0)
			{
				st.ctrl_type = E_CTRL_CANCEL;
				//getRtsp->UserCancelStream(id_frame, task_req, sock, thread_no);
			}
			else
			{
				LOG(INFO) << "Only support ctrl-task of CANCEL type! but recv a " << task_req.ctrl_flag << " type!";
				upRspMessage->sendResponse(id_frame, sock, "Only support ctrl-task of CANCEL type!");
				zframe_destroy(&id_frame);
				continue;
			}

			bool bFind(false);
			//EnterCriticalSection(&CGlobalSettings::csMapCtrl);
			std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
			auto it = CGlobalSettings::mapTaskID2Ctrl.find(task_req.task_id);
			if (it != CGlobalSettings::mapTaskID2Ctrl.end())
			{
				it->second = st;
				bFind = true;
			}
			//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
			lck.unlock();

			if (!bFind)  /*指定要控制的任务不存在*/
			{
				LOG(INFO) << "Ctrl task for cancel not exist!";
				//TASK_CTRL_INTER_RSP rsp;
				//memcpy(rsp.task_id, strTaskID.c_str(), strTaskID.size() + 1);
				//memcpy(rsp.result, "fail", sizeof("fail"));
				//send_rsp_2_task_ctrl(id_frame, sock, rsp);
				upRspMessage->sendResponse(id_frame, sock, "Fail: Ctrl task for cancel not exist!");
			}else  //只能是取消任务
				getRtsp->UserCancelStream(id_frame, task_req, sock, thread_no);
		}
		else if (E_BIZ_TYPE_ASR == emReqType)
		{
			/* 语音检测收到一个语音识别的消息*/
			LOG(INFO) << "Voice check module recieve an asr request,please check!";
			//noise_send_2_request(id_frame, sock, rsp, strTaskID.c_str(), E_MSG_TYPE_JSON_ERROR);
			upRspMessage = unique_ptr<CRecogResponse>(new CRecogResponse(strTaskID));
			upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_JSON_ERROR);
		}
		else if (E_BIZ_TYPE_CHECK == emReqType)
		{
			LOG(INFO) << "ProcessNoiseCheck req_type ：E_BIZ_TYPE_CHECK";
			//decode_str_2_check_req(item,req );

			/* 接收成功，返回一个成功消息 */
			//EnterCriticalSection(&g_csCheck[thread_no]);
			//noise_send_2_request(id_frame, sock, rsp, strTaskID.c_str(), E_MSG_TYPE_ACCEPT_SUCC);
			//LeaveCriticalSection(&g_csCheck[thread_no]);

			upRspMessage = unique_ptr<CCheckResponse>(new CCheckResponse(strTaskID));
			upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_ACCEPT_SUCC);

			/* 对加密狗进行检验 */
			//EnterCriticalSection(&g_csCheck[thread_no]);
			int lic_ret = CLicense::GetInstance().Status(0);
			if (CLicense::LIC_OK != lic_ret) /* 如果返回HASP的状态不为OK，则返回给用户错误码*/
			{
				//LOG(LOG_DEBUG, "Encrypted dog authentication failure,status[%d],thread_id[%d]", lic_ret, GetCurrentThreadId());
				LOG(INFO) << "Encrypted dog authentication failure,status[" << lic_ret << "],thread_id[" << thread_id << "]";

				/* 返回请求方相关信息 */
				//sprintf(rsp.msg, "Encrypted dog authentication failure,status:[%d]", lic_ret);
				//noise_send_2_request(id_frame, sock, rsp, req.task_id, E_MSG_TYPE_DOG_ERROR);
				//LeaveCriticalSection(&g_csCheck[thread_no]);

				upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_DOG_ERROR);

				zframe_destroy(&id_frame);
			}
			//LeaveCriticalSection(&g_csCheck[thread_no]);

			st_task_ctrl task_ctrl;
			task_ctrl.ctrl_type = E_CTRL_NONE;
			task_ctrl.thread_no = thread_no;

			/* 如果是非任务控制业务，则以task_id为主键，把信息存放到map中，以便发任务取消时使用 */
			//EnterCriticalSection(&CGlobalSettings::csMapCtrl);
			std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
			auto it = CGlobalSettings::mapTaskID2Ctrl.find(strTaskID);
			if (it == CGlobalSettings::mapTaskID2Ctrl.end())
			{
				CGlobalSettings::mapTaskID2Ctrl.insert(make_pair(strTaskID, task_ctrl));
			}
			//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
			lck.unlock();

			/* 进入语音质量检测处理程序 */
			VOICE_CHECK_REQ req = { 0 };
			JsonParser.GetCheckRequest(req);
			getRtsp->SetRtspUrl(req.media_url);
			LOG(INFO) << "recv rtsp url: " << req.media_url;
			bRet = getRtsp->asr_voice_quality_detection(id_frame, &req, sock, thread_no);

			/* 处理完毕，把相应task_id信息从map中擦除 */
			//EnterCriticalSection(&CGlobalSettings::csMapCtrl);
			//std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
			lck.lock();
			auto iter = CGlobalSettings::mapTaskID2Ctrl.find(strTaskID);
			if (iter != CGlobalSettings::mapTaskID2Ctrl.end())
			{
				/* 如果任务取消已经成功，则不会进入。如果没有任务取消，则要把map中相关数据删除 */
				if (iter->second.ctrl_type != E_CTRL_SUCC)
					CGlobalSettings::mapTaskID2Ctrl.erase(iter);
			}
			//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
			lck.unlock();

			/* 发送完毕，返回一个处理完毕的消息 */
			//EnterCriticalSection(&g_csCheck[thread_no]);
			//noise_send_2_request(id_frame, sock, rsp1, req.task_id, E_MSG_TYPE_SEND_FINISH);
			//LeaveCriticalSection(&g_csCheck[thread_no]);
			if (bRet)
				upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_SEND_FINISH);
			else
				upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_BIZ_FAIL);
		}// end of E_BIZ_TYPE_CHECK == emReqType
		else //unkown type
		{
			LOG(INFO) << "------------Receive an unkown type request Msg, ignore it------------";
			upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_JSON_ERROR);
		}

		free(content_str);
		zframe_destroy(&id_frame);
	}//end of while()

	//LOG( LOG_DEBUG,"Leave %s process....!");

	zmq_close(sock);
}

void ProcessVoiceDetector(void *context, string addr, void *args )
{
	assert(!addr.empty());

	int			iRet(-1);
	bool		bRet(false);
	string		strTaskID;
	E_BIZ_TYPE	emReqType(E_BIZ_TYPE_UNKOWN);

	int thread_no = (int)args;
	int thread_id = GetCurrentThreadId();

	LOG( INFO )<<"Enter "<<__FUNCTION__<<" process....!addr:["<<addr<<"]";

	void *sock = zmq_socket(context, ZMQ_DEALER);
	if (nullptr == sock)
	{
		LOG(INFO) << "speech recognize module:create recv connect socket error:" << zmq_strerror(zmq_errno());
		return;
	}

#if (defined (WIN32))
	s_set_id(sock, (intptr_t)args);
#else
	s_set_id(sock);          //  Set a printable identity
#endif

	cout << "speech recognizee start success! current thread no is[ " << thread_no << " ],thread_id: "<<thread_id << endl;
	LOG(INFO) << "current thread number: " << thread_no << "， thread id:" << thread_id;

	//void *sock = zmq_socket(context, ZMQ_DEALER);
	iRet = zmq_connect(sock, addr.c_str());
	if (iRet == -1)
	{
		LOG(INFO) << "zmq_connect error!error:[" << zmq_strerror(zmq_errno())<<"]";
		zmq_close(sock);
		return;
	}

	unique_ptr<GetRtspUrlData> getRtsp(new GetRtspUrlData);

	while (true)
	{
		zmsg_t *msg = zmsg_recv(sock);
		if (!msg) {
			LOG( INFO )<< "Worker[ "<<thread_no<<" ] :Failed received request message";
			return;
		}

		//保存源地址id帧和数据帧
		zframe_t *id_frame		= zmsg_pop(msg);		//弹出帧头
		zframe_t *null_frame	= zmsg_pop(msg);		//弹出空帧
		zframe_t *content_frame = zmsg_pop(msg);		//弹出数据帧

		char *content_str = zframe_strdup(content_frame);

		zmsg_destroy( &msg );
		zframe_destroy(&null_frame);
		zframe_destroy(&content_frame);

		//int  con_len		= zframe_size(content_frame);
		//char *id_str		= zframe_strhex(id_frame);

		LOG(INFO) << "Recog thread received request from the " << thread_no << "'s client whose thread:" << thread_id << " data[" << content_str << "]";

		CMessageParser JsonParser;
		unique_ptr<CResponseMsg> upRspMessage;

		//item = decode_task_ctrl_str(id_frame,content_str,(void*)&req, emReqType);

		JsonParser.Init(content_str);

		emReqType = JsonParser.GetReqMsgType();
		strTaskID = JsonParser.GetReqTaskID();

		/* 如果请求类型为控制类型，则进入任务控制处理函数 */
		if (E_BIZ_TYPE_CTRL == emReqType)
		{
			LOG(INFO) << "ProcessVoiceDetector req_type: E_BIZ_TYPE_CTRL";

			upRspMessage = unique_ptr<CCtrlResponse>(new CCtrlResponse(strTaskID));

			TASK_CONTROL_INTERFACE task_req;
			st_task_ctrl st;
			st.thread_no = thread_no;
			//decode_str_2_control_req(id_frame, item, task_req);
			JsonParser.GetCtrlReq(task_req);

			if (stricmp(task_req.ctrl_flag, "cancel") == 0)
			{
				st.ctrl_type = E_CTRL_CANCEL;
			}
			else
			{
				LOG(INFO) << "Only support ctrl-task of CANCEL type! but recv a " << task_req.ctrl_flag<< " type!";
				zframe_destroy(&id_frame);
				continue;
			}

			bool bFind(false);
			//EnterCriticalSection(&CGlobalSettings::csMapCtrl);
			std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
			auto it = CGlobalSettings::mapTaskID2Ctrl.find(task_req.task_id);
			if (it != CGlobalSettings::mapTaskID2Ctrl.end())
			{
				it->second = st;
				bFind = true;
			}
			//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
			lck.unlock();

			if (!bFind)  /*指定要控制的任务不存在*/
			{
				LOG(INFO) << "Ctrl task  in mapTaskID2Ctrl for cancel not exist!";
				//TASK_CTRL_INTER_RSP rsp;
				//memcpy(rsp.task_id, strTaskID.c_str(), strTaskID.size() + 1);
				//memcpy(rsp.result, "fail", sizeof("fail"));
				//send_rsp_2_task_ctrl(id_frame, sock, rsp);
				upRspMessage->sendResponse(id_frame, sock, "Fail: Ctrl task for cancel not exist!");
			}else
				getRtsp->UserCancelStream(id_frame, task_req, sock, thread_no);
		}
		else if (E_BIZ_TYPE_CHECK == emReqType)
		{
			/* 此处应该报一个类型错误，不过这种情况不会出现*/
			LOG(INFO) << "Speech recog module receive a voice check request,please check";
			upRspMessage = unique_ptr<CCheckResponse>(new CCheckResponse(strTaskID));
			upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_JSON_ERROR);
		}
		else if(E_BIZ_TYPE_ASR == emReqType)
		{
			//decode_str_2_detector_req(item, req);

			upRspMessage = unique_ptr<CRecogResponse>(new CRecogResponse(strTaskID));
			upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_ACCEPT_SUCC);

			/* 对加密狗进行检验 */
			int lic_ret = CLicense::GetInstance().Status(0);
			if (CLicense::LIC_OK != lic_ret) /* 如果返回HASP的状态不为OK，则返回给用户错误码*/
			{
				LOG(INFO) << "Encrypted dog authentication failure,status[" << lic_ret << "],thread_id[" << thread_id << "]";

				//AUDIO_DETECTOR_RSP rsp;
				/* 返回请求方相关信息 */
				//sprintf(rsp.msg, "Encrypted dog authentication failure,status:[%d]", lic_ret);
				//vad_send_2_request(id_frame, sock, rsp, req.task_id, E_MSG_TYPE_DOG_ERROR);

				upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_DOG_ERROR);

				zframe_destroy(&id_frame);
				continue;
			}

			st_task_ctrl task_ctrl;
			task_ctrl.ctrl_type = E_CTRL_NONE;		//原来代码写为0，也就是E_CTRL_PAUSE，这很奇怪
			task_ctrl.thread_no = thread_no;

			//EnterCriticalSection(&CGlobalSettings::csMapCtrl);
			std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
			auto it = CGlobalSettings::mapTaskID2Ctrl.find(strTaskID);
			if (it == CGlobalSettings::mapTaskID2Ctrl.end())
			{
				CGlobalSettings::mapTaskID2Ctrl.insert(make_pair(strTaskID, task_ctrl));
			}
			//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
			lck.unlock();

			/* 接收成功，返回一个成功消息 */
			//AUDIO_DETECTOR_RSP rsp;
			//vad_send_2_request(id_frame, sock, rsp, req.task_id, E_MSG_TYPE_ACCEPT_SUCC);

			AUDIO_DETECTOR_REQ req = { 0 };
			JsonParser.GetRecogRequest(req);
			getRtsp->SetRtspUrl(req.media_url);
			LOG(INFO) << "recv rtsp url:" << req.media_url << endl;
			bRet = getRtsp->asr_speech_recognition(id_frame, &req, sock, thread_no);

			/* 处理完毕，把相应task_id信息从map中擦除 */
			//EnterCriticalSection(&CGlobalSettings::csMapCtrl);
			//std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
			lck.lock();
			auto iter = CGlobalSettings::mapTaskID2Ctrl.find(req.task_id);
			if (iter != CGlobalSettings::mapTaskID2Ctrl.end())
			{
				if (iter->second.ctrl_type != E_CTRL_SUCC)
				{
					CGlobalSettings::mapTaskID2Ctrl.erase(iter);
				}
			}
			//LeaveCriticalSection(&CGlobalSettings::csMapCtrl);
			lck.unlock();

			//AUDIO_DETECTOR_RSP rsp_fin;
			//vad_send_2_request(id_frame, sock, rsp_fin, req.task_id, E_MSG_TYPE_SEND_FINISH);
			if (bRet)
				upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_SEND_FINISH);
			else
				upRspMessage->sendResponse(id_frame, sock, E_MSG_TYPE_BIZ_FAIL);

			LOG(INFO) << "speech recognize process finish!task id[" << req.task_id << "] thread_no[" << thread_no << "]";

		}
		else //unkown type
			LOG(INFO) << "------------Receive an unkown type request Msg, ignore it------------";

		free(content_str);
		zframe_destroy(&id_frame);
	}

	//zmq_msg_close(&msg);

	LOG( INFO )<< "Leave "<<__FUNCTION__<<" process....!";

	zmq_close(sock);
}

/* 接收VAD请求，该函数不再被需要 */
void RecvVADRequest( char *buffer , AUDIO_DETECTOR_REQ *req, void *sock )
{
	zmq_msg_t recv_msg;
	zmq_msg_init(&recv_msg);
	int ret = zmq_msg_recv(&recv_msg, sock, 0);
	int msg_size = zmq_msg_size(&recv_msg);
	memcpy(buffer, zmq_msg_data(&recv_msg), msg_size);

	zmq_msg_t nul_msg;
	zmq_msg_init(&nul_msg);
	ret = zmq_msg_recv(&recv_msg, sock, 0);

	zmq_msg_t data_msg;
	zmq_msg_init(&data_msg);
	ret = zmq_msg_recv(&data_msg, sock, 0);
	zmq_msg_close(&data_msg);

	int size = sizeof(*req);
	char Data[1024] = { 0 };
	msg_size = zmq_msg_size(&data_msg);
	::memcpy(Data, zmq_msg_data(&data_msg), msg_size);
	req = (AUDIO_DETECTOR_REQ*)Data;
	zmq_msg_close(&data_msg);

	return;
}


/* 给请求方发送响应，该函数不再被需要 */
void Send2VADRequest(char *dstAddr, AUDIO_DETECTOR_RSP *rsp, void *sock,int snd_flags)
{
	int snd_size;

	if (snd_flags == 1)
	{
		zmq_msg_t msg;
		snd_size = strlen(dstAddr) + 1;
		int rc = zmq_msg_init_size(&msg, snd_size);
		::memcpy(zmq_msg_data(&msg), dstAddr, snd_size);
		rc = zmq_msg_send(&msg, sock, 0);
		zmq_msg_close(&msg);

		zmq_msg_t nul_msg;
		rc = zmq_msg_send(&msg, "", 1);
		zmq_msg_close(&nul_msg);
	}

	int snd = 0;
	if (snd_flags == 2)
	{
		snd = ZMQ_SNDMORE;
	}
	else
	{
		snd = 0;
	}

	zmq_msg_t data_msg;
	snd_size = sizeof(AUDIO_DETECTOR_RSP);
	int rc = zmq_msg_init_size(&data_msg, snd_size);

	::memcpy(zmq_msg_data(&data_msg), rsp, snd_size);

	rc = zmq_msg_send(&data_msg, sock, snd);

	zmq_msg_close(&data_msg);

	return;
}
