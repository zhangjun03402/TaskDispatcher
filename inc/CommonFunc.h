#pragma once

#include "ChRTSP.h"
#include "PublicHead.h"
#include "libczmq\include\czmq.h"
#include "json\value.h"

using namespace Json;

typedef void(*callback)(char *srcfile, int srcline, int level, char *fmt, ...);

void Bytes2Shorts(char* cBuf, int cBufLen, short* sBuf);

void Shorts2Floats(short* buf, int len, float *fBuf);

string noisereq2string(VOICE_CHECK_REQ *req);
string noisersp2string(VOICE_CHECK_RSP *rsp);
string audio_req_2_string(AUDIO_DETECTOR_REQ *req);
string audio_rsp_2_string(AUDIO_DETECTOR_RSP*rsp);

string decode_data_from_asr(string str);
Value decode_task_ctrl_str(zframe_t *id_frame, string str, void *req, E_BIZ_TYPE &req_type);
void decode_str_2_detector_req(Json::Value str, AUDIO_DETECTOR_REQ &req);
void decode_str_2_control_req(IN zframe_t *id_frame, IN Json::Value str, IN OUT TASK_CONTROL_INTERFACE &req);
void decode_str_2_check_req(Json::Value str, VOICE_CHECK_REQ &req);

string encode_task_ctrl_2_req(TASK_CTRL_INTER_RSP &rsp);
void send_rsp_2_task_ctrl(zframe_t *id_frame,void*sock,TASK_CTRL_INTER_RSP &rsp);

VOICE_CHECK_REQ string2noisereq(string str);
VOICE_CHECK_RSP string2noisersp(string str);
AUDIO_DETECTOR_REQ string_2_audioreq(string str);
AUDIO_DETECTOR_RSP string_2_audiorsp(string str);

void str_2_check_req(string str, VOICE_CHECK_REQ &req);
void str_2_detector_req(string str, AUDIO_DETECTOR_REQ &req);

string detector_rsp_2_str(AUDIO_DETECTOR_RSP &rsp);
string noise_rsp_2_str(VOICE_CHECK_RSP &rsp);
void noise_send_2_request(zframe_t *id_frame, void *sock, VOICE_CHECK_RSP &rsp, const char* task_id, int msg_type = 0);
void vad_send_2_request(zframe_t *id_frame, void *sock, AUDIO_DETECTOR_RSP &rsp, char* task_id, int msg_type);


