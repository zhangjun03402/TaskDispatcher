#ifndef __HT_LOG_H__
#define __HT_LOG_H__

/* 日志文件名的长度的最大值 */
#define LOG_NAME_LEN_MAX          32

#define HT_LOG_MODE_OFF           0
#define HT_LOG_MODE_ERROR         1
#define HT_LOG_MODE_NORMAL        2
#define HT_LOG_MODE_DEBUG         3
#define HT_LOG_MODE_TEMP          4

/* 功能:写日志文件
 * 参数:
 *      sLogName   --[in]日志名,不带路径
 *      nLogMode   --[in]日志级别: HT_LOG_MODE_XXX
 *      sFileName  --[in]报错的源程序名
 *      nLine      --[in]报错的源程序的行号
 *      sFmt       --[in]错误信息格式
 *      ...        --[in]变量信息
 * 返回值:
 *      无
 */
void HtLog(const char *sLogName, int nLogMode, const char *sFileName, int nLine, char *sFmt, ...);

/*****************************************************************************/
/* FUNC:   int HtDebugString (char *sLogName, int nLogMode, char *sFileName, */
/*                            int nLine, char *psBuf, int nBufLen);          */
/* INPUT:  sLogName: 日志名, 不带路径                                        */
/*         nLogMode: 日志级别,                                               */
/*                   HT_LOG_MODE_ERROR,HT_LOG_MODE_NORMAL,HT_LOG_MODE_DEBUG  */
/*         sFileName: 报错的源程序名                                         */
/*         nLine: 报错的源程序的行号                                         */
/*         psBuf: 需输出的buffer                                             */
/*         nBufLen: buffer的长度                                             */
/* OUTPUT: 无                                                                */
/* RETURN: 0: 成功, 其它: 失败                                               */
/* DESC:   根据LOG_MODE, 将该级别之下的日志记录到日志文件中,                 */
/*         输出内容是buffer的16进制值                                        */
/*****************************************************************************/
int HtDebugString(char *sLogName, int nLogMode, char *sFileName, int nLine, char *psBuf, int nBufLen);

#endif
