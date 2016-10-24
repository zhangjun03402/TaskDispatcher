#ifndef LOG_H_GB985JK345HJI345HJKL345HJIO234
#define LOG_H_GB985JK345HJI345HJKL345HJIO234

#include <string>

namespace Lingban
{
	namespace Common
	{

		const int LOG_MODE_SCREEN_ONLY = 0;  //����Ļ���
		const int LOG_MODE_BOTH        = 1;  //��Ļ+�ļ�
		const int LOG_MODE_FILE_ONLY   = 2;  //���ļ����

		//��־�������
		const int LOG_DEBUG  = 0;
		const int LOG_INFO   = 1;
		const int LOG_NOTICE = 2;
		const int LOG_WARN   = 3;
		const int LOG_ERROR  = 4;
		const int LOG_PANIC  = 5;
		const int LOG_DIRECT = 6;

		#define LOG(log_level,fmt, ...) CLog::__LOG(__FILE__,__LINE__,log_level,fmt,__VA_ARGS__)

		//////////////////////////////////////////////////////////////////////////
		// CLog
		//////////////////////////////////////////////////////////////////////////
class CLog
{
    //static void *LogRoutine(void *param);
public:
    /*������־���*/
    static void SetupLog(const char *path, const char *prefix, int flush_size, int archive_size, int mode);
    /*������־�������*/
    static int SetLevel(int level);
    /*ֹͣ�ļ���־��¼*/
    static void Stop();

    static int GetLevelByName(char *name);

    static void Flush();

    static void __LOG(char *srcfile, int srcline, int log_level, char* fmt, ...);

    CLog(void);

    ~CLog(void);

    int SetupLogBroadcast(const std::string &broker_address, const std::string &filter_id_str);

    int SetupSyslogClient(const std::string &server_address);
};
//////////////////////////////////////////////////////////////////////////

}}
#endif