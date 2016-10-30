#pragma once
#include "PublicHead.h"
#include "libczmq\include\czmq.h"
#include "MessageParser.h"
#include <memory>
#ifdef __cplusplus
extern "C" {
#endif

	void ProcessNoiseCheck(void *context, std::string addr, void *args);
	void ProcessVoiceDetector(void *context, std::string addr, void *args);
	void worker_noise_check(string args, int nbr);
	void worker_speech_recog(string args, int nbr);

	void RecvVADRequest(char *dstAddr, AUDIO_DETECTOR_REQ *req, void *sock);
	void Send2VADRequest(char *dstAddr, AUDIO_DETECTOR_RSP *rsp, void *sock,int snd_flags);

#ifdef __cplusplus
}
#endif

class CTaskDispatcher {
public:

private:

	zframe_t * _FrameID;


};