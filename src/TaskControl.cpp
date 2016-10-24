/* 该文本主要是实现任务控制相关的功能，启动代理，处理代理相关的代码 */
#include "..\inc\PublicHead.h"
#include "..\inc\TaskControl.h"
#include "..\inc\CommonFunc.h"
//#include "..\inc\zhelpers.h"
#include "libczmq\include\czmq.h"
//#include "inc\log.h"
#include "glog\logging.h"

#include <assert.h>

#include <iostream>
using namespace std;

//extern zmq::context_t zmq_ctx ;
/* 语音质量检测相关的代理实现，创建代理 */
void TaskControl::NoiseRoutine( )
{
	//const char *noise_front_bind = "tcp://*:18000";
	void *front_socket = zmq_socket(zmq_ctx, ZMQ_ROUTER);
	if (!front_socket) {
		//LOG( LOG_DEBUG,"Setting UP ZMQ TTS Broker: Creating Front Socket to %s Failed, error = ", noise_front_bind, zmq_strerror(zmq_errno()));
		LOG(INFO) << "Creating Audio Check Front Socket to " << noise_front_bind << " Failed, error=" << zmq_strerror(zmq_errno());
		return;
	}
	else {
		//LOG( LOG_DEBUG,"Setting UP ZMQ TTS Broker: Binding Front Socket to %s", noise_front_bind);
		LOG( INFO)<<"Create Audio Check Front Socket to "<<noise_front_bind<<" success";
	}

	int bind_result = zmq_bind(front_socket, noise_front_bind);
	if (bind_result != 0) {
		//LOG( LOG_DEBUG,"Failed Binding front socket %s, errno = %s", noise_front_bind, zmq_strerror(zmq_errno()));
		LOG( INFO)<<"Failed Binding front socket "<<noise_front_bind<<" errno="<< zmq_strerror(zmq_errno());
		return;
	}

	void *back_socket = zmq_socket(zmq_ctx, ZMQ_DEALER);
	if (!back_socket) {
		//LOG( LOG_DEBUG,"Setting UP ZMQ TTS Broker: Creating Back Socket to %s Failed, error = ", noise_back_bind, zmq_strerror(zmq_errno()));
		LOG( INFO )<<"Creating Back Socket to "<< noise_back_bind<<" Failed, error = "<< zmq_strerror(zmq_errno());
		return;
	}
	else {
		//LOG(LOG_DEBUG, "Setting UP ZMQ TTS Broker: Binding Back Socket to %s", noise_back_bind);
		LOG(INFO)<< "Binding Back Socket to "<<noise_back_bind<<" success";
	}

	bind_result = zmq_bind(back_socket, noise_back_bind);
	if (bind_result != 0) {
		//LOG( LOG_DEBUG,"Failed Binding back socket %s, errno = %s", noise_back_bind, zmq_strerror(zmq_errno()));
		LOG( INFO ) <<"Failed Binding to back socket "<< noise_back_bind<<", errno = %s"<< zmq_strerror(zmq_errno());
		return;
	}

	zmq_proxy(front_socket, back_socket, NULL);

	LOG(INFO) << "start audio state check proxy success";
	//LOG(LOG_DEBUG, "start noise check proxy success!");
	cout << "start noise check proxy success" << endl;

	//  We never get here, but clean up anyhow
	zmq_close(front_socket);
	front_socket = nullptr;

	zmq_close(back_socket);
	back_socket = nullptr;

	return;
}

/* 语音识别相关的实现，创建代理 */
void TaskControl::RecogRoutine( )
{
	void *front_socket = zmq_socket(zmq_ctx, ZMQ_ROUTER);
	if (!front_socket) {
		//LOG( LOG_DEBUG,"Setting UP ZMQ TTS Broker: Creating Front Socket to %s Failed, error = ", recog_front_bind, zmq_strerror(zmq_errno()));
		LOG(INFO) << "Create voice recognize front socket to " << recog_front_bind << " failed,error=" << zmq_strerror(zmq_errno());
		return;
	}
	else {
		//LOG( LOG_DEBUG,"Setting UP ZMQ TTS Broker: Binding Front Socket to %s", recog_front_bind);
		LOG( INFO )<<"Create voice recognize front socket to "<< recog_front_bind<<" success";
	}

	int bind_result = zmq_bind(front_socket, recog_front_bind);
	if (bind_result != 0) {
		//LOG( LOG_DEBUG,"Failed Binding front socket %s, errno = %s", recog_front_bind, zmq_strerror(zmq_errno()));
		LOG( INFO)<<"Failed binding to voice recognize front socket "<<recog_front_bind<<" errno="<< zmq_strerror(zmq_errno());
		return;
	}

	void *back_socket = zmq_socket(zmq_ctx, ZMQ_DEALER);
	if (!back_socket) {
		//LOG( LOG_DEBUG,"Setting UP ZMQ TTS Broker: Creating Back Socket to %s Failed, error = ", recog_back_bind, zmq_strerror(zmq_errno()));
		LOG( INFO )<<"Creating voice recognize Back Socket to "<<recog_back_bind<<" Failed, error="<<zmq_strerror(zmq_errno());
		return;
	}
	else {
		//LOG( LOG_DEBUG,"Setting UP ZMQ TTS Broker: Binding Back Socket to %s", recog_back_bind);
		LOG(INFO) << "Create voice recognize back socket to " << recog_back_bind << " success";
	}
	bind_result = zmq_bind(back_socket, recog_back_bind);
	if (bind_result != 0) {
		//LOG( LOG_DEBUG,"Failed Binding back socket %s, errno = %s", recog_back_bind, zmq_strerror(zmq_errno()));
		LOG( INFO )<<"Failed binding back socket "<< recog_back_bind<<", errno = %s"<< zmq_strerror(zmq_errno());
		return;
	}

	zmq_proxy(front_socket, back_socket, NULL);

	//LOG(LOG_DEBUG, "start noise check proxy success!");
	LOG(INFO)<< "start voice recognize proxy success!";

	//  We never get here, but clean up anyhow
	zmq_close(front_socket);
	front_socket = nullptr;

	zmq_close(back_socket);
	back_socket = nullptr;

}

/*创建一个代理,分别调用噪音检测和语音识别*/
void TaskControl::BrokerRoutine()
{
	//std::thread tCheck(std::mem_fn(&TaskControl::NoiseRoutine), this);
	//std::thread tRecog(std::mem_fn(&TaskControl::RecogRoutine), this);
	
}

TaskControl::TaskControl( void* context )
{
	zmq_ctx = context;
	//if ((zmq_ctx = zmq_ctx_new()) == NULL)
	//{
	//	cout << "create context error" << endl;
	//}
}

TaskControl::~TaskControl()
{
	zmq_ctx_destroy(zmq_ctx);
}







