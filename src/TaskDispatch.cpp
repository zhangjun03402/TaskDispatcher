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
#include <csignal>
using namespace std ;
#pragma comment(lib, "Ws2_32.lib")

//using namespace Lingban::Common;
using namespace Lingban::CoreLib;

extern std::mutex			g_mtxMapCtrl;
extern condition_variable	g_ConVar;

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

void worker_noise_check(string addr, int no)
{
	zctx_t *ctx = zctx_new();
	void *worker_check = zsocket_new(ctx, ZMQ_DEALER);
	
	char cThreadID[50];
	snprintf(cThreadID, 50, "threadid_%d", GetCurrentThreadId());
	zmq_setsockopt(worker_check, ZMQ_IDENTITY, cThreadID, sizeof(cThreadID));
#if (defined (WIN32))
	//zsocket_connect(worker, "tcp://localhost:18001"); // backend
	zsocket_connect(worker_check, addr.c_str()); // backend
#else
	zsocket_connect(worker_check, "ipc://backend.ipc");
#endif
	
	//  Tell broker we're ready for work
	zmsg_t *msg_ready = zmsg_new();
	zmsg_addmem(msg_ready, NULL, 0);			// delimiter
	zframe_t *frame_ready = zframe_new(CHECK_WORKER_READY, strlen(CHECK_WORKER_READY));
	zmsg_append(msg_ready, &frame_ready);
	zmsg_send(&msg_ready, worker_check);
	assert(msg_ready == NULL);
	assert(frame_ready == NULL);

	bool		bRet(false);
	int			thread_no = no;
	int			thread_id = GetCurrentThreadId();
	E_BIZ_TYPE	emReqType;
	string		strTaskID;

	unique_ptr<GetRtspUrlData>	getRtsp(new GetRtspUrlData);

	while (true)
	{
		zmsg_t *msg = zmsg_recv(worker_check);
		if (!msg)
			break;              //  Interrupted

		LOG(INFO) << "thread: " << cThreadID << " recv msg : " << zmsg_size(msg);

		//保存源地址id帧和数据帧
		zframe_t *null_frame1 = zmsg_pop(msg);		//弹出空帧
		zframe_t *id_frame = zmsg_pop(msg);			//弹出id帧
		zframe_t *null_frame2 = zmsg_pop(msg);		//弹出空帧
		zframe_t *content_frame = zmsg_pop(msg);	//弹出数据帧

		char *content_str = zframe_strdup(content_frame);
		//zframe_t *id_copy = zframe_dup(id_frame);

		zmsg_destroy(&msg);
		zframe_destroy(&null_frame1);
		zframe_destroy(&null_frame2);
		zframe_destroy(&content_frame);

		LOG(INFO) << "Check thread received request from the " << thread_no << "'s client whose thread:" << thread_id << " data[" << content_str << "]";

		CMessageParser JsonParser;
		JsonParser.Init(content_str);
		emReqType = JsonParser.GetReqMsgType();
		strTaskID = JsonParser.GetReqTaskID();

		unique_ptr<CResponseMsg> upRspMessage = unique_ptr<CCheckResponse>(new CCheckResponse(strTaskID));

		/* 如果是非任务控制业务，则以task_id为主键，把信息存放到map中，以便发任务取消时使用 */
		st_task_ctrl task_ctrl;
		task_ctrl.ctrl_type = E_CTRL_NONE;
		task_ctrl.thread_no = thread_no;
		std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
		auto it = CGlobalSettings::mapTaskID2Ctrl.find(strTaskID);
		if (it == CGlobalSettings::mapTaskID2Ctrl.end())
		{
			CGlobalSettings::mapTaskID2Ctrl.insert(make_pair(strTaskID, task_ctrl));
		}
		lck.unlock();

		/* 进入语音质量检测处理程序 */
		VOICE_CHECK_REQ req = { 0 };
		JsonParser.GetCheckRequest(req);
		getRtsp->SetRtspUrl(req.media_url);
		LOG(INFO) << "recv rtsp url: " << req.media_url;
		//函数调用里遇到id_frame的地方都要zframe_dup();
		bRet = getRtsp->asr_voice_quality_detection(id_frame, &req, worker_check, thread_no);

		/* 正常处理完毕(如果没有被取消)，把相应task_id信息从map中擦除 */
		lck.lock();
		auto iter = CGlobalSettings::mapTaskID2Ctrl.find(strTaskID);
		if (iter != CGlobalSettings::mapTaskID2Ctrl.end())
		{
			if (iter->second.ctrl_type != E_CTRL_SUCC)
				CGlobalSettings::mapTaskID2Ctrl.erase(iter);
		}
		lck.unlock();

		//确保此时id_frame是有效的
		if (bRet)
			upRspMessage->sendResponse(id_frame, worker_check, E_MSG_TYPE_SEND_FINISH);
		else
			upRspMessage->sendResponse(id_frame, worker_check, E_MSG_TYPE_BIZ_FAIL);

		//  Tell broker we're ready for work
		msg_ready = zmsg_new();
		zmsg_addmem(msg_ready, NULL, 0);			// delimiter
		//zmsg_append(msg_ready, &id_copy);
		//zmsg_addmem(msg_ready, NULL, 0);			// delimiter
		frame_ready = zframe_new(CHECK_WORKER_READY, strlen(CHECK_WORKER_READY));
		zmsg_append(msg_ready, &frame_ready);

		zmsg_send(&msg_ready, worker_check);
		assert(frame_ready == NULL);
	} // while
	zctx_destroy(&ctx);
	return;
}

void worker_speech_recog(string addr, int no)
{
	zctx_t *ctx = zctx_new();
	void *worker_recog = zsocket_new(ctx, ZMQ_DEALER);
	char cThreadID[50];
	snprintf(cThreadID, 50, "threadid_%d", GetCurrentThreadId());
	zmq_setsockopt(worker_recog, ZMQ_IDENTITY, cThreadID, sizeof(cThreadID));
#if (defined (WIN32))
	//zsocket_connect(worker, "tcp://localhost:18001"); // backend
	zsocket_connect(worker_recog, addr.c_str()); // backend
#else
	zsocket_connect(worker, "ipc://backend.ipc");
#endif

	zmsg_t *msg_ready = zmsg_new();
	zmsg_addmem(msg_ready, NULL, 0);			// delimiter
	zframe_t *frame_ready = zframe_new(CHECK_WORKER_READY, strlen(CHECK_WORKER_READY));
	zmsg_append(msg_ready, &frame_ready);
	zmsg_send(&msg_ready, worker_recog);
	assert(msg_ready == NULL);
	assert(frame_ready == NULL);

	bool		bRet(false);
	int			thread_no = no;
	int			thread_id = GetCurrentThreadId();
	E_BIZ_TYPE	emReqType;
	string		strTaskID;

	unique_ptr<GetRtspUrlData>	getRtsp(new GetRtspUrlData);

	while (true)
	{
		zmsg_t *msg = zmsg_recv(worker_recog);
		if (!msg)
			break;              //  Interrupted

		LOG(INFO) << "thread: " << cThreadID << " recv msg : " << zmsg_size(msg);

		//保存源地址id帧和数据帧
		zframe_t *null_frame1 = zmsg_pop(msg);		//弹出空帧
		zframe_t *id_frame = zmsg_pop(msg);			//弹出id帧
		zframe_t *null_frame2 = zmsg_pop(msg);		//弹出空帧
		zframe_t *content_frame = zmsg_pop(msg);	//弹出数据帧

		char *content_str = zframe_strdup(content_frame);
		//zframe_t *id_copy = zframe_dup(id_frame);

		zmsg_destroy(&msg);
		zframe_destroy(&null_frame1);
		zframe_destroy(&null_frame2);
		zframe_destroy(&content_frame);

		LOG(INFO) << "Check thread received request from the " << thread_no << "'s client whose thread:" << thread_id << " data[" << content_str << "]";

		CMessageParser JsonParser;
		JsonParser.Init(content_str);
		free(content_str);

		emReqType = JsonParser.GetReqMsgType();
		strTaskID = JsonParser.GetReqTaskID();

		unique_ptr<CResponseMsg> upRspMessage = unique_ptr<CCheckResponse>(new CCheckResponse(strTaskID));

		/* 如果是非任务控制业务，则以task_id为主键，把信息存放到map中，以便发任务取消时使用 */
		st_task_ctrl task_ctrl;
		task_ctrl.ctrl_type = E_CTRL_NONE;
		task_ctrl.thread_no = thread_no;
		std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
		auto it = CGlobalSettings::mapTaskID2Ctrl.find(strTaskID);
		if (it == CGlobalSettings::mapTaskID2Ctrl.end())
		{
			CGlobalSettings::mapTaskID2Ctrl.insert(make_pair(strTaskID, task_ctrl));
		}
		lck.unlock();

		/* 进入语音识别处理程序 */
		AUDIO_DETECTOR_REQ req = { 0 };
		JsonParser.GetRecogRequest(req);
		getRtsp->SetRtspUrl(req.media_url);
		LOG(INFO) << "recv rtsp url:" << req.media_url << endl;
		bRet = getRtsp->asr_speech_recognition(id_frame, &req, worker_recog, thread_no);
		/* 处理完毕，如果没有被取消，把相应task_id信息从map中擦除 */
		lck.lock();
		auto iter = CGlobalSettings::mapTaskID2Ctrl.find(strTaskID);
		if (iter != CGlobalSettings::mapTaskID2Ctrl.end())
		{
			if (iter->second.ctrl_type != E_CTRL_SUCC)
				CGlobalSettings::mapTaskID2Ctrl.erase(iter);
		}
		lck.unlock();

		//确保此时id_frame是有效的
		if (bRet)
			upRspMessage->sendResponse(id_frame, worker_recog, E_MSG_TYPE_SEND_FINISH);
		else
			upRspMessage->sendResponse(id_frame, worker_recog, E_MSG_TYPE_BIZ_FAIL);

		//  Tell broker we're ready for work
		msg_ready = zmsg_new();
		zmsg_addmem(msg_ready, NULL, 0);			// delimiter
													//zmsg_append(msg_ready, &id_copy);
													//zmsg_addmem(msg_ready, NULL, 0);			// delimiter
		frame_ready = zframe_new(CHECK_WORKER_READY, strlen(CHECK_WORKER_READY));
		zmsg_append(msg_ready, &frame_ready);

		zmsg_send(&msg_ready, worker_recog);
		assert(frame_ready == NULL);
	} // while

	zctx_destroy(&ctx);
	return;
}