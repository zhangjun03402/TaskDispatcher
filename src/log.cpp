#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <windows.h>
#include <vector>
#include <map>
#include <fcntl.h>
#include <io.h>
#include <list>
#include <utility>
#include <vector>
#include <direct.h>
#include <string>
#include <thread>
#include <mutex>
#include "inc\log.h"

using namespace Lingban::Common;

#ifdef WIN32
const WORD LOGCOLOR_FWHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const WORD LOGCOLOR_FDARKWHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
const WORD LOGCOLOR_FRED = FOREGROUND_RED | FOREGROUND_INTENSITY;
const WORD LOGCOLOR_FDARKRED = FOREGROUND_RED;
const WORD LOGCOLOR_FGREEN = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
const WORD LOGCOLOR_FDARKGREEN = FOREGROUND_GREEN;
const WORD LOGCOLOR_FBLUE = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const WORD LOGCOLOR_FDARKBLUE = FOREGROUND_BLUE;
const WORD LOGCOLOR_FMAGEN = FOREGROUND_BLUE | FOREGROUND_RED;
const WORD LOGCOLOR_FCYAN = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const WORD LOGCOLOR_FYELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
const WORD LOGCOLOR_DEFAULT_COLOR = LOGCOLOR_FWHITE;
const WORD LOGCOLOR_MEET_EYE = LOGCOLOR_FDARKBLUE | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY;
const WORD LOGCOLOR_CATCH_EYE = LOGCOLOR_FWHITE | BACKGROUND_RED;
const WORD LOGCOLOR_DROP_EYE = LOGCOLOR_FYELLOW | FOREGROUND_INTENSITY | BACKGROUND_RED;

const WORD LOG_COLOR_DEBUG  = LOGCOLOR_FCYAN;
const WORD LOG_COLOR_INFO   = LOGCOLOR_FWHITE;
const WORD LOG_COLOR_NOTICE = LOGCOLOR_MEET_EYE;
const WORD LOG_COLOR_WARN   = LOGCOLOR_FYELLOW;
const WORD LOG_COLOR_ERROR  = LOGCOLOR_CATCH_EYE;
const WORD LOG_COLOR_PANIC  = LOGCOLOR_DROP_EYE;
const WORD LOG_COLOR_DIRECT = LOGCOLOR_FBLUE;

static WORD LogColors[7] = {    
    LOG_COLOR_DEBUG,
    LOG_COLOR_INFO,
    LOG_COLOR_NOTICE,
    LOG_COLOR_WARN,
    LOG_COLOR_ERROR,
    LOG_COLOR_PANIC,
    LOG_COLOR_DIRECT
};

static WORD initital_screen = LOGCOLOR_FWHITE;
static HANDLE h_out = NULL;
#endif

const int LOG_BUFF_SIZE = (1024 * 1024);
const int LOG_FLUSH_SIZE = (1024 * 64);
const int LOG_ARCHIEVE_SIZE =  (1024 * 1024 * 32);


//static FILE* log_fp = stderr; 
static char *log_path = ".\\";
static char *log_prefix = "biloba_tts";

static std::mutex        log_mutex;
static std::thread       log_thread;
static int log_running = 0;
static bool flush_required = false;

static char logbuff[LOG_BUFF_SIZE];
static int  log_flush_size = LOG_FLUSH_SIZE;
static int  log_archive_size = LOG_ARCHIEVE_SIZE;
static int  n_inbuf = 0;
static int  log_mode = LOG_MODE_SCREEN_ONLY;
static bool log_on_screen = true;

static bool log_print_src_info = true;
static int current_loglevel = LOG_DEBUG;
const char *log_level_string[8] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "NOTICE",
    "WARN",
    "ERROR",
    "CRITIC",
    ""
};


CLog::CLog(void)
{
}

CLog::~CLog(void)
{
}


//--------------------------------------------------------------
// log routines
//--------------------------------------------------------------
int CLog::GetLevelByName(char *name)
{
    for (int i = 0; i<8; i++){
        if (strcmp(log_level_string[i], name) == 0){
            return i;
        }
    }
    return -1;

}


/******************************************************************************
* vsyslog
*
* Generate a log message using FMT and using arguments pointed to by AP.
*/

int CLog::SetLevel(int level)
{
    current_loglevel = level;
    if (current_loglevel <LOG_DEBUG){
        current_loglevel = LOG_DEBUG;
    } else if (current_loglevel>LOG_PANIC){
        current_loglevel = LOG_PANIC;
    }
    return current_loglevel;
}

std::string get_time_str(const char *format)
{
    time_t t = time(NULL);
    char buffer[50];
    strftime(buffer, 50, format, localtime(&t));
    return std::string(buffer);
}

void *LogRoutine();

void CLog::SetupLog(const char *path, const char *prefix, int flush_size, int archive_size, int mode)
{
    //void *log_flush(void *param);

    log_mutex.lock();   

    if (mode <= LOG_MODE_FILE_ONLY && mode >= LOG_MODE_SCREEN_ONLY){
        log_mode = mode;
    } else{
        log_mode = LOG_MODE_SCREEN_ONLY;
    }


    if (!h_out){
        CONSOLE_SCREEN_BUFFER_INFO bInfo; // 存储窗口信息
        h_out = GetStdHandle(STD_ERROR_HANDLE);
        // 获取窗口信息
        GetConsoleScreenBufferInfo(h_out, &bInfo);
        initital_screen = bInfo.wAttributes;
    }

    if (path)
        log_path = strdup(path);
    _mkdir(log_path);

    if (prefix)
        log_prefix = strdup(prefix);

    if (flush_size>0 && flush_size <= LOG_FLUSH_SIZE){
        log_flush_size = flush_size;
    } else{
        log_flush_size = LOG_FLUSH_SIZE;
    }

    if (archive_size>0 && archive_size < (1024 * 1024 * 512)){
        log_archive_size = archive_size;
    } else{
        log_archive_size = LOG_ARCHIEVE_SIZE;
    }

    if (log_running == 0 && log_mode >= LOG_MODE_BOTH){        
        log_thread = std::thread(LogRoutine);        
    }
    log_mutex.unlock();

    return;

}


void CLog::Stop()
{
    bool running = false;
    void *ptr;
    log_mutex.lock();
    if (log_running == 1){
        log_running = 2;
        running = true;
    }
    log_mutex.unlock();
    if (running)
        log_thread.join();
        
    log_mode = LOG_MODE_SCREEN_ONLY;
}

//force flushing the log to file
void CLog::Flush()
{
    log_mutex.lock();
    flush_required = true;
    log_mutex.unlock();
}

void *LogRoutine()
{
    char log_fn[MAX_PATH];
    char log_fn_ar[MAX_PATH];
    long file_size = 0;
    log_running = 1;
    flush_required = false;
    while (log_running >0){
        //log_mutex.unlock();
        if (n_inbuf>log_flush_size || log_running == 2 || (flush_required && n_inbuf>0)){
            sprintf(log_fn, "%s\\%s.log", log_path, log_prefix);
            FILE *fp_log = fopen(log_fn, "ab");
            if (fp_log){
                fwrite(logbuff, sizeof(char), n_inbuf, fp_log);
                file_size = ftell(fp_log);
                fclose(fp_log);
                n_inbuf = 0;
                flush_required = false;
            }else{
                n_inbuf = 0;
                flush_required = false;            
            }
            if (file_size >= log_archive_size){
#ifdef WIN32
                SYSTEMTIME stm;
                GetLocalTime(&stm);

                sprintf(log_fn_ar, "%s\\%s_%04d%02d%02d_%02d%02d%02d.log",
                    log_path, log_prefix,
                    stm.wYear, stm.wMonth, stm.wDay,
                    stm.wHour, stm.wMinute, stm.wSecond);

#else
                time_t timep;
                struct tm *p;
                time(&timep);
                p = localtime(&timep);   //get server's time

                sprintf(log_fn_ar, "%s\\%s_%04d%02d%02d_%02d%02d02d.log",
                    log_path, log_prefix,
                    p->tm_year + 1900, p->tm_mon + 1, p->tm_mday,
                    p->tm_hour, p->tm_min, p->tm_sec);

#endif
                rename(log_fn, log_fn_ar);
                fp_log = fopen(log_fn, "w");
                fclose(fp_log);
            }
        }
        //log_mutex.unlock();
        if (log_running != 1){
            break;
        } else{
            Sleep(100);
        }
    }

    log_running = 0;
    return NULL;
}


//#define FORCR_LOG_DEBUG

void vsyslog(char *srcfile, int srcline, unsigned log_level, char* fmt, va_list ap)
{


#ifdef FORCR_LOG_DEBUG
    static FILE *fp = NULL;
    if (!fp){
        fp = fopen("c:\\forcelog.log", "at");
    }
#endif

    static char *month[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static char tmp_src_f[512];
    static char srcstr[512];
    static char datagramm[65536];
    char *rest;
    SYSTEMTIME stm;
    int len;

    if (!h_out){
        CONSOLE_SCREEN_BUFFER_INFO bInfo; // 存储窗口信息
        h_out = GetStdHandle(STD_ERROR_HANDLE);
        // 获取窗口信息
        GetConsoleScreenBufferInfo(h_out, &bInfo);
        initital_screen = bInfo.wAttributes;
    }

    log_mutex.lock();

    if (log_level == LOG_DIRECT){
        vsnprintf(datagramm, 1024, fmt, ap);
        if (log_mode <= LOG_MODE_BOTH){
            SetConsoleTextAttribute(h_out, LogColors[log_level]);
            fprintf(stderr, "%s\n", datagramm);
            SetConsoleTextAttribute(h_out, initital_screen);
        }
        if (log_mode >= LOG_MODE_BOTH && log_running == 1){
            int slen = strlen(datagramm);
            memcpy(logbuff + n_inbuf, datagramm, slen);
            n_inbuf += slen;
            logbuff[n_inbuf] = 0x0d;
            logbuff[n_inbuf + 1] = 0x0a;
            n_inbuf += 2;
        }
    } else if (log_level >= current_loglevel){
        if (log_level <LOG_DEBUG){
            log_level = LOG_DEBUG;
        } else if (log_level>LOG_PANIC){
            log_level = LOG_PANIC;
        }

        strcpy(tmp_src_f, srcfile);
        for (int l = strlen(tmp_src_f) - 1; l>0; l--){
            if (tmp_src_f[l] == '.'){
                tmp_src_f[l] = '\0';
                break;
            }
        }
        if (log_print_src_info){
            sprintf(srcstr, "(%s:%04d) ", tmp_src_f, srcline);
        } else{
            srcstr[0] = '\0';
        }

        GetLocalTime(&stm);
        len = sprintf(datagramm, "%02d-%02d %02d:%02d:%02d.%03d %s[%6s]: ",
            stm.wMonth, stm.wDay, stm.wHour, stm.wMinute, stm.wSecond, stm.wMilliseconds,
            srcstr, log_level_string[log_level]);
        vsnprintf(datagramm + len, 65535 - len, fmt, ap);
        rest = datagramm;

#ifdef FORCR_LOG_DEBUG
        if (fp){
            fprintf(fp, "%s\n", datagramm);
            fflush(fp);
        }

#endif

#if 0
        p = strchr(datagramm, '\n');
        if (p) *p = 0;

        p = strchr(datagramm, '\r');
        if (p) *p = 0;
#endif
        if (log_mode <= LOG_MODE_BOTH){
            SetConsoleTextAttribute(h_out, LogColors[log_level]);
            fprintf(stderr, "%s\n", datagramm);
            SetConsoleTextAttribute(h_out, initital_screen);
        }


        if (log_mode >= LOG_MODE_BOTH && log_running == 1){
            int slen = strlen(datagramm);
            memcpy(logbuff + n_inbuf, datagramm, slen);
            n_inbuf += slen;
            logbuff[n_inbuf] = 0x0d;
            logbuff[n_inbuf + 1] = 0x0a;
            n_inbuf += 2;
        }
    }
    log_mutex.unlock();
}

/******************************************************************************
* syslog
*
* Generate a log message using FMT string and option arguments.
*/
void CLog::__LOG(char *srcfile, int srcline, int log_level, char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsyslog(srcfile, srcline, log_level, fmt, ap);
    va_end(ap);
}


