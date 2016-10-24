#pragma once

//#include "..\inc\zhelpers.h"
#include <thread>
#include <vector>

class TaskControl
{
public:
	TaskControl(void *context);
	~TaskControl();
		
	void BrokerRoutine( ); /* �������� */
	void NoiseRoutine(  ); /* ������������������*/
	void RecogRoutine(  ); /* ��������ʶ�����*/
	
private:
	void						*zmq_ctx;     //������
	std::vector<std::thread>	m_vectThreads;

	/* ���������� */
	const char *noise_front_bind = "tcp://*:18000";
	const char *noise_back_bind  = "tcp://*:18001";

	/* ����ʶ����� */
	const char *recog_front_bind = "tcp://*:17000";
	const char *recog_back_bind  = "tcp://*:17001";
};
