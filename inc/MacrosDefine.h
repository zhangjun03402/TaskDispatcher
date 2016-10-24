#ifndef _MACROS_DEFINE_
#define _MACROS_DEFINE_

#define M_PI 3.14159265358979323846
#define LOG_FMT ("__FILE__","__LINE__")

//函数参数传入、传出标识
#define IN
#define OUT

//配置文件和日志
#define CONFIG_PATH		".\\config\\"
#define CONFIG_FILE		"configure"
#define LOG_PATH		".\\log\\"

//地址
#define DETECT_BROKER_BACK_ADDR		"check_conn_back_addr"
#define RECOGNIZE_BROKER_BACK_ADDR	"asr_conn_back_addr"
#define ASR_SERVER_ADDR				"asr_server_addr"

//json消息字段
#define TASK_ID				"task_id"
#define MESSAGE				"message"
#define SEQUENCE			"sequence"
#define STATUS_CODE			"status_code"
#define START_TIME			"start_time"
#define END_TIME			"end_time"
#define DURATION			"duration"
#define TEXT_				"text"
#define RESULT				"result"
#define MEDIA_URL			"media_url"



#endif // !_MACROS_DEFINE_
