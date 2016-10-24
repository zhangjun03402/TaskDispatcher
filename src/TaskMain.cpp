#include "..\inc\TaskControl.h"
#include "..\inc\CommonFunc.h"
#include "..\inc\TaskDispatch.h"
#include "..\inc\GetRtspUrlData.h"
#include "..\inc\ChRTSP.h"
#include "lic.h"

#include "glog\logging.h"
#include "GlobalSettings.h"
#include "ThreadSafeQueue.h"

#include <queue>
#include <map>
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <stdio.h>
#include <stdlib.h>

using namespace std;
using namespace Lingban::CoreLib;

threadsafe_queue<st_msg_queue> g_msg_queue_check[Max_Threads];  //语音质量检测全局消息列表
threadsafe_queue<st_msg_queue> g_msg_queue_asr[Max_Threads];

std::mutex			g_mtxCheckMsgQ[Max_Threads];  //主意质量检测临界区
std::mutex			g_mtxMapCtrl;
condition_variable	g_ConVar;

//using namespace Lingban::Common;

extern CRITICAL_SECTION cs_log;

#define VERSION_ "lb_v_0.1"
int main(int argc, char *argv[])
{

//#ifdef WIN32
//	InitializeCriticalSection(&cs_log);
//#else
//	pthread_mutex_init(&cs_log, NULL);
//#endif

	bool	bRet;
	int		iRet;
	string	strAddr;

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

	/* set log print*
	* SetupLog(const char *path, const char *prefix, int flush_size, int archive_size, int mode) 
	*/
	//CLog::SetupLog("log","lingban",1024, 1024*1024, LOG_MODE_BOTH);
	//CLog::SetLevel(LOG_DEBUG);

	//register_log(CLog::__LOG);

	/* 初始化加密狗 */
	iRet = CLicense::GetInstance().Start(200, 30);

	LOG(INFO) << "Init Encrypted dog authentication ret:"<< iRet;

	//LOG(INFO) << "Init configure files success,noise_addr:" << conn_noise_back_addr <<" recog_addr:"<< conn_recog_back_addr;

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
	iRet = CH_RTSP_Init(NULL);
	if (iRet != 0)
	{
		LOG(INFO) << "Init RTSP failed! Please check whether the service is already started!";
		return EXIT_FAILURE;
	}

	void *context = zmq_ctx_new();
	if (nullptr == context)
	{
		LOG(INFO)<< "create zmq context error!err:["<< zmq_strerror(zmq_errno())<<"]";
		return -1;
	}

	LOG(INFO) << "Create zmq context success";

	/* new任务对象 */
	shared_ptr<TaskControl> spTaskCtrl = make_shared<TaskControl>(context);
	if (nullptr == spTaskCtrl)
	{
		LOG(INFO) << "new task control is null";
		return 0;
	}

	LOG(INFO) << "new task control success";

	/* 创建语音质量检测线程 */
	auto  Noise_broker = make_shared<thread>([spTaskCtrl] {spTaskCtrl->NoiseRoutine();});

	/* 创建语音识别检测线程 */
	auto Recog_broker = make_shared<thread>([spTaskCtrl] {spTaskCtrl->RecogRoutine();});

	srandom((unsigned)time(NULL));

	/* 创建连接语音质量检测线程，处理噪音检测发送过来的请求，并把处理结果返回给请求方 */
	strAddr = CGlobalSettings::GetInstance().GetDetectBrokerBackAddr();
	vector<shared_ptr<thread>> Noise_workers;
	for (auto i=0; i < Max_Threads; i++ )
	{
		auto conn_noise_back = make_shared<thread>(ProcessNoiseCheck, context, /*conn_noise_back_addr*/strAddr, (void *)(intptr_t)i);
		Noise_workers.emplace_back( conn_noise_back );
	}
	
	LOG(INFO) << "Create voice check threads success";

	/* 创建连接语音识别线程池，处理语音识别类请求，并把处理结果返回给请求方 */
	strAddr = CGlobalSettings::GetInstance().GetRecogBrokerBackAddr();
	vector<shared_ptr<thread>> Recog_workers;
	for (auto i = 0; i < Max_Threads; i++)
	{
		auto conn_recog_back = make_shared<thread>(ProcessVoiceDetector, context,  /*conn_recog_back_addr*/strAddr, (void *)(intptr_t)i);
		Recog_workers.emplace_back(conn_recog_back);
	}

	LOG(INFO) << "Create speech recognize threads success";

	if (Noise_broker)
	{
		Noise_broker->join();
	}
	if (Recog_broker)
	{
		Recog_broker->join();
	}

	for ( auto conn_noise_back:Noise_workers )
	{
		conn_noise_back->join();
	}
	for ( auto conn_recog_back:Recog_workers )
	{
		conn_recog_back->join();
	}

	DeleteCriticalSection(&CGlobalSettings::csMapCtrl);


//#ifdef WIN32
//	DeleteCriticalSection(&cs_log);
//#else
//	pthread_mutex_destroy(&cs_log);
//#endif
//
	CH_RTSP_UnInit();
	LOG(INFO) << "Program exit...";
	google::ShutdownGoogleLogging();

	return EXIT_SUCCESS;
}