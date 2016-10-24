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

threadsafe_queue<st_msg_queue> g_msg_queue_check[Max_Threads];  //�����������ȫ����Ϣ�б�
threadsafe_queue<st_msg_queue> g_msg_queue_asr[Max_Threads];

std::mutex			g_mtxCheckMsgQ[Max_Threads];  //������������ٽ���
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

	// ��ʼ������ȡ�����ļ�����õ�ַ:�˿���Ϣ�������־�ļ����Ƿ����
	bRet = CGlobalSettings::GetInstance().Initialize();
	if (!bRet) {
		cerr << "Program config failed!" << endl;
		return EXIT_FAILURE;
	}

	/* ��ʼ����־��ӡ */
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

	/* ��ʼ�����ܹ� */
	iRet = CLicense::GetInstance().Start(200, 30);

	LOG(INFO) << "Init Encrypted dog authentication ret:"<< iRet;

	//LOG(INFO) << "Init configure files success,noise_addr:" << conn_noise_back_addr <<" recog_addr:"<< conn_recog_back_addr;

	/* ��ʼ���ٽ���(ȫ��map����)����Ҫ���������ʱҪʹ�� */
	InitializeCriticalSection(&CGlobalSettings::csMapCtrl);
	LOG(INFO) << "Init CriticalSection CGlobalSettings::csMapCtrl success";

	for ( int i=0;i< Max_Threads;i++ )
	{
		//��������ע��Ϊzj�鿴����²�
		rtsp_status[i]		= 0;	//�������Ҫ��Ϊ0���ɹ���������һ������ʱ״̬�ص����������񡱲������á�
		rtsp_asr_status[i]	= -1;	//����ʶ����Ϊ-1û���⣬����һ������ʱ��Ӧ�á����޸�״̬Ϊ0���ɹ�����
	}

	/* ��ʼ�����ĵ�ͨ���� */
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

	/* new������� */
	shared_ptr<TaskControl> spTaskCtrl = make_shared<TaskControl>(context);
	if (nullptr == spTaskCtrl)
	{
		LOG(INFO) << "new task control is null";
		return 0;
	}

	LOG(INFO) << "new task control success";

	/* ����������������߳� */
	auto  Noise_broker = make_shared<thread>([spTaskCtrl] {spTaskCtrl->NoiseRoutine();});

	/* ��������ʶ�����߳� */
	auto Recog_broker = make_shared<thread>([spTaskCtrl] {spTaskCtrl->RecogRoutine();});

	srandom((unsigned)time(NULL));

	/* ��������������������̣߳�����������ⷢ�͹��������󣬲��Ѵ��������ظ����� */
	strAddr = CGlobalSettings::GetInstance().GetDetectBrokerBackAddr();
	vector<shared_ptr<thread>> Noise_workers;
	for (auto i=0; i < Max_Threads; i++ )
	{
		auto conn_noise_back = make_shared<thread>(ProcessNoiseCheck, context, /*conn_noise_back_addr*/strAddr, (void *)(intptr_t)i);
		Noise_workers.emplace_back( conn_noise_back );
	}
	
	LOG(INFO) << "Create voice check threads success";

	/* ������������ʶ���̳߳أ���������ʶ�������󣬲��Ѵ��������ظ����� */
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