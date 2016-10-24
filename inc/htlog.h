#ifndef __HT_LOG_H__
#define __HT_LOG_H__

/* ��־�ļ����ĳ��ȵ����ֵ */
#define LOG_NAME_LEN_MAX          32

#define HT_LOG_MODE_OFF           0
#define HT_LOG_MODE_ERROR         1
#define HT_LOG_MODE_NORMAL        2
#define HT_LOG_MODE_DEBUG         3
#define HT_LOG_MODE_TEMP          4

/* ����:д��־�ļ�
 * ����:
 *      sLogName   --[in]��־��,����·��
 *      nLogMode   --[in]��־����: HT_LOG_MODE_XXX
 *      sFileName  --[in]�����Դ������
 *      nLine      --[in]�����Դ������к�
 *      sFmt       --[in]������Ϣ��ʽ
 *      ...        --[in]������Ϣ
 * ����ֵ:
 *      ��
 */
void HtLog(const char *sLogName, int nLogMode, const char *sFileName, int nLine, char *sFmt, ...);

/*****************************************************************************/
/* FUNC:   int HtDebugString (char *sLogName, int nLogMode, char *sFileName, */
/*                            int nLine, char *psBuf, int nBufLen);          */
/* INPUT:  sLogName: ��־��, ����·��                                        */
/*         nLogMode: ��־����,                                               */
/*                   HT_LOG_MODE_ERROR,HT_LOG_MODE_NORMAL,HT_LOG_MODE_DEBUG  */
/*         sFileName: �����Դ������                                         */
/*         nLine: �����Դ������к�                                         */
/*         psBuf: �������buffer                                             */
/*         nBufLen: buffer�ĳ���                                             */
/* OUTPUT: ��                                                                */
/* RETURN: 0: �ɹ�, ����: ʧ��                                               */
/* DESC:   ����LOG_MODE, ���ü���֮�µ���־��¼����־�ļ���,                 */
/*         ���������buffer��16����ֵ                                        */
/*****************************************************************************/
int HtDebugString(char *sLogName, int nLogMode, char *sFileName, int nLine, char *psBuf, int nBufLen);

#endif
