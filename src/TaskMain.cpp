#include "..\inc\TaskControl.h"
#include "..\inc\CommonFunc.h"
#include "..\inc\TaskDispatch.h"
#include "..\inc\GetRtspUrlData.h"
#include "..\inc\ChRTSP.h"
#include "lic.h"

#include "glog\logging.h"
#include "GlobalSettings.h"
#include "ThreadSafeQueue.h"
#include "MessageParser.h"

#include <queue>
#include <map>
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <cstdio>
#include <cstdlib>

using namespace std;
using namespace Lingban::CoreLib;

threadsafe_queue<st_msg_queue> g_msg_queue_check[Max_Threads];  //语音质量检测全局消息列表
threadsafe_queue<st_msg_queue> g_msg_queue_asr[Max_Threads];

std::mutex			g_mtxMapCtrl;
condition_variable	g_ConVar;

//using namespace Lingban::Common;

int main(int argc, char *argv[])
{
	bool	bRet;

	// 初始化：读取配置文件，获得地址:端口信息；检测日志文件夹是否存在
	bRet = CGlobalSettings::GetInstance().Initialize();
	if (!bRet) {
		cerr << "Program config failed!" << endl;
		return EXIT_FAILURE;
	}

	/* 初始化日志打印 */
	google::InitGoogleLogging((const char*)argv[0]);
	google::SetLogDestination(google::GLOG_INFO, LOG_PATH);
	FLAGS_logbufsecs = 0;
	//google::SetLogFilenameExtension("");
	
	LOG(INFO) << "Init log modules success";
	LOG(INFO) << "noise check back addr:" << CGlobalSettings::GetInstance().GetDetectBrokerBackAddr() << "! recog back addr:" << CGlobalSettings::GetInstance().GetRecogBrokerBackAddr();
	LOG(INFO) << "ASR Server Addr: " << CGlobalSettings::GetInstance().GetASRServerAddr();

	/* 初始化加密狗 */
	int iRet = CLicense::GetInstance().Start(200, 30);
	if (!iRet) {
		LOG(INFO) << "Init Encrypted Dog Failed!";
		cout << "Init Encrypted Dog Failed!" << endl;
		google::ShutdownGoogleLogging();
		return EXIT_FAILURE;
	}

	/* 初始化临界区(全局map控制)，主要是任务控制时要使用 */
	InitializeCriticalSection(&CGlobalSettings::csMapCtrl);
	LOG(INFO) << "Init CriticalSection CGlobalSettings::csMapCtrl success";

	for ( int i=0;i< Max_Threads;i++ )
	{
		//下面两行注释为zj查看代码猜测
		rtsp_status[i]		= 0;	//语音检测要设为0（成功），它第一次拉流时状态回调函数“好像”不起作用。
		rtsp_asr_status[i]	= -1;	//语音识别设为-1没问题，它第一次拉流时“应该”会修改状态为0（成功）。
	}

	/* 初始化华夏电通拉流 */
	CH_RTSP_Init(NULL);

	zctx_t *ctx = zctx_new();
	void *frontend = zsocket_new(ctx, ZMQ_ROUTER);
	void *backend = zsocket_new(ctx, ZMQ_ROUTER);

	// IPC doesn't yet work on MS Windows.
#if (defined (WIN32))
	zsocket_bind(frontend, "tcp://*:18000");
	zsocket_bind(backend, "tcp://*:18001");
#else
	zsocket_bind(frontend, "ipc://frontend.ipc");
	zsocket_bind(backend, "ipc://backend.ipc");
#endif

	vector<shared_ptr<thread>> vect_worker_threads;
	/*语音检测线程*/
	string strAddr = CGlobalSettings::GetInstance().GetDetectBrokerBackAddr();
	for (int nbr = 0; nbr < NBR_CHECK_WORKERS; ++nbr)
	{
		auto conn_noise_back = make_shared<thread>(worker_noise_check, strAddr, nbr);
		vect_worker_threads.emplace_back(conn_noise_back);
	}
	/*语音识别线程*/
	strAddr = CGlobalSettings::GetInstance().GetRecogBrokerBackAddr();
	for (int nbr = 0; nbr < NBR_RECOG_WORKERS; nbr++)
	{
		auto conn_recog_back = make_shared<thread>(worker_speech_recog, strAddr, nbr);
		vect_worker_threads.emplace_back(conn_recog_back);
	}

	int nCheckThreads(0), nRecogThreads(0);
	string			strTaskID;
	E_BIZ_TYPE		emReqType(E_BIZ_TYPE_UNKOWN);

	unique_ptr<GetRtspUrlData>	getRtsp(new GetRtspUrlData);
	unique_ptr<CResponseMsg>	upRspMessage;

	//  Queue of available workers
	zlist_t *check_worker_queue = zlist_new();
	zlist_t *recog_worker_queue = zlist_new();

	while (true) {
		zmq_pollitem_t items[] = {
			{ backend, 0, ZMQ_POLLIN, 0 },
			{ frontend, 0, ZMQ_POLLIN, 0 }
		};

		int rc = zmq_poll(items, 2, -1);
		if (rc == -1)
			break;              //  Interrupted

		//  Handle worker activity on backend
		if (items[0].revents & ZMQ_POLLIN) {
			//  Use worker identity for load-balancing
			zmsg_t *msg = zmsg_recv(backend);
			if (!msg)
				break;          //  Interrupted
			
			zframe_t *id_worker = zmsg_pop(msg);
			zframe_t *delimiter = zmsg_pop(msg);
			zframe_destroy(&delimiter);
			char *str_worker = zframe_strdup(id_worker);
			LOG(INFO) << "Msg from worker thread: " << str_worker;
			free(str_worker);

			// Forward message to client if it's not a READY
			zframe_t *first_content_frame = zmsg_first(msg);

			if (memcmp(zframe_data(first_content_frame), CHECK_WORKER_READY, strlen(CHECK_WORKER_READY)) == 0) {	// check
				zlist_append(check_worker_queue, id_worker);
				zmsg_destroy(&msg);
				continue;
			}
			else if(memcmp(zframe_data(first_content_frame), RECOG_WORKER_READY, strlen(RECOG_WORKER_READY)) == 0){	// recognize
				zlist_append(recog_worker_queue, id_worker);
				zmsg_destroy(&msg);
				continue;
			}
			zframe_destroy(&id_worker);

			//Forward left msg to frontend
			zmsg_send(&msg, frontend);

		} //if (items[0].revents & ZMQ_POLLIN)

		if (items[1].revents & ZMQ_POLLIN) {
			//  Get client request, route to first available worker
			zmsg_t *msg = zmsg_recv(frontend);
			if (!msg)
				break;          //  Interrupted

			zframe_t *worker_frame = NULL;

			zframe_t *id_frame = zmsg_pop(msg);			// client id
			zframe_t *null_frame = zmsg_pop(msg);		// null
			zframe_t *content_frame = zmsg_pop(msg);	// client request

			char *content_str = zframe_strdup(content_frame);
			zframe_t *id_copy = zframe_dup(id_frame);	// client id

			CMessageParser JsonParser;
			JsonParser.Init(content_str);

			zframe_destroy(&null_frame);
			zmsg_destroy(&msg);
			free(content_str);

			emReqType = JsonParser.GetReqMsgType();
			strTaskID = JsonParser.GetReqTaskID();

			if (E_BIZ_TYPE_CTRL == emReqType)
			{
				LOG(INFO) << "req_type E_BIZ_TYPE_CTRL";
				upRspMessage = unique_ptr<CCtrlResponse>(new CCtrlResponse(strTaskID));
				//zframe_t *id_copy = zframe_dup(id_frame);
				upRspMessage->sendResponse(zframe_dup(id_frame), frontend, E_MSG_TYPE_ACCEPT_SUCC, false);

				TASK_CONTROL_INTERFACE task_req;

				JsonParser.GetCtrlReq(task_req);
				if (stricmp(task_req.ctrl_flag, "cancel") == 0)
				{
					bool bFind(false);
					std::unique_lock<std::mutex> lck(g_mtxMapCtrl);
					auto it = CGlobalSettings::mapTaskID2Ctrl.find(task_req.task_id);
					if (it != CGlobalSettings::mapTaskID2Ctrl.end())
					{
						it->second.ctrl_type = E_CTRL_CANCEL;
						bFind = true;
					}
					lck.unlock();

					if (!bFind)  /*指定要控制的任务不存在*/
					{
						LOG(INFO) << "Ctrl task for cancel not exist!";
						upRspMessage->sendResponse(id_frame, frontend, "Fail: Ctrl task for cancel not exist!");
					}
					else {
						bRet = getRtsp->UserCancelStream(task_req);
						if (bRet) {
							upRspMessage->sendResponse(id_frame, frontend, "success");
							std::cout << "task cancel success!task_id:" << task_req.task_id;
							LOG(INFO) << "task control:business cancel success!task_id:" << task_req.task_id;
						}
						else {
							upRspMessage->sendResponse(id_frame, frontend, "fail");
							std::cout << "task cancel fail!task_id:" << task_req.task_id;
							LOG(INFO) << "task control:business cancel fail!task_id:" << task_req.task_id;
						}
					}
				}
				else
				{
					LOG(INFO) << "Only support ctrl-task of CANCEL type! but recv a " << task_req.ctrl_flag << " type!";
					upRspMessage->sendResponse(id_frame, frontend, "Only support ctrl-task of CANCEL type!");
				}
				//zframe_destroy(&id_frame);	//无需释放，消息发送时自动释放
				//zframe_destroy(&id_copy);
				continue;		//跳过后面check和asr的执行
			}
			else if (E_BIZ_TYPE_CHECK == emReqType)
			{
				LOG(INFO) << "req_type ：E_BIZ_TYPE_CHECK";
				upRspMessage = unique_ptr<CCheckResponse>(new CCheckResponse(strTaskID));
				
				if (0 == zlist_size(check_worker_queue)) {
					upRspMessage->sendResponse(id_frame, frontend, E_MSG_TYPE_SERVER_BUSY, false);
					continue;
				}

				/* 对加密狗进行检验 */
				//int lic_ret = CLicense::GetInstance().Status(0);
				//if (CLicense::LIC_OK != lic_ret)
				//{
				//	LOG(INFO) << "Encrypted dog authentication failure,status[" << lic_ret << "]";
				//	upRspMessage->sendResponse(id_frame, frontend, E_MSG_TYPE_DOG_ERROR, false);
				//	zframe_destroy(&id_frame);
				//	continue;
				//}

				upRspMessage->sendResponse(id_frame, frontend, E_MSG_TYPE_ACCEPT_SUCC, false);

				worker_frame = (zframe_t *)zlist_pop(check_worker_queue);
			}
			else if (E_BIZ_TYPE_ASR == emReqType)
			{
				LOG(INFO) << " req_type ：E_BIZ_TYPE_ASR";
				upRspMessage = unique_ptr<CRecogResponse>(new CRecogResponse(strTaskID));

				if (0 == zlist_size(recog_worker_queue)) {
					upRspMessage->sendResponse(id_frame, frontend, E_MSG_TYPE_SERVER_BUSY, false);
					continue;
				}
				/* 加密狗检验 */
				int lic_ret = CLicense::GetInstance().Status(0);
				if (CLicense::LIC_OK != lic_ret)
				{
					LOG(INFO) << "Encrypted dog authentication failure,status[ " << lic_ret << " ]";
					upRspMessage->sendResponse(id_frame, frontend, E_MSG_TYPE_DOG_ERROR, false);
					continue;
				}
				upRspMessage->sendResponse(id_frame, frontend, E_MSG_TYPE_ACCEPT_SUCC, false);

				worker_frame = (zframe_t *)zlist_pop(recog_worker_queue);
			}
			else
			{
				LOG(INFO) << "------------Receive an unkown type request Msg, ignore it------------";
				upRspMessage->sendResponse(id_frame, frontend, E_MSG_TYPE_JSON_ERROR, false);
				continue;
			}

			zmsg_t *msg1 = zmsg_new();
			zmsg_push(msg1, content_frame);
			zmsg_pushmem(msg1, NULL, 0);
			zmsg_push(msg1, id_copy);			// client
			zmsg_pushmem(msg1, NULL, 0);		// delimiter
			zmsg_push(msg1, worker_frame);		// worker

			zmsg_send(&msg1, backend);			// send
		} //if (items[1].revents & ZMQ_POLLIN)
	} // while

	//  When we're done, clean up properly
	while (zlist_size(check_worker_queue)) {
		zframe_t *frame = (zframe_t *)zlist_pop(check_worker_queue);
		zframe_destroy(&frame);
	}
	while (zlist_size(check_worker_queue)) {
		zframe_t *frame = (zframe_t *)zlist_pop(check_worker_queue);
		zframe_destroy(&frame);
	}

	for (auto thrd : vect_worker_threads)
	{
		thrd->join();
	}

	zlist_destroy(&check_worker_queue);
	zlist_destroy(&recog_worker_queue);
	zctx_destroy(&ctx);

	DeleteCriticalSection(&CGlobalSettings::csMapCtrl);

	CH_RTSP_UnInit();
	LOG(INFO) << "Program exit...";
	google::ShutdownGoogleLogging();

	return EXIT_SUCCESS;
}