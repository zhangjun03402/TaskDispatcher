#pragma once

//#include "..\inc\zhelpers.h"
#include <thread>
#include <vector>

class TaskControl
{
public:
	TaskControl(void *context);
	~TaskControl();
		
	void BrokerRoutine( ); /* 创建代理 */
	void NoiseRoutine(  ); /* 创建语音质量检测代理*/
	void RecogRoutine(  ); /* 创建语音识别代理*/
	
private:
	void						*zmq_ctx;     //上下文
	std::vector<std::thread>	m_vectThreads;

	/* 噪音检测进程 */
	const char *noise_front_bind = "tcp://*:18000";
	const char *noise_back_bind  = "tcp://*:18001";

	/* 语音识别进程 */
	const char *recog_front_bind = "tcp://*:17000";
	const char *recog_back_bind  = "tcp://*:17001";
};
