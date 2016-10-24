#ifndef LIC_H_XJU49CDGG898
#define LIC_H_XJU49CDGG898

#include <string>
#include <mutex>
#include <thread>
#include "hasp_api.h"

namespace Lingban{namespace CoreLib{

typedef int lic_result_t;
class CLicense{
private:		
    std::mutex mtx;
    std::thread th;
    static unsigned char lingban_vendor_code[];

    bool ready;
    int  feature_id;
    int  n_permissions;
    int  status;
    bool debug_mode;
    char error_string[1024];
    clock_t watch_dog_clock = 0;
    bool exiting;
    bool init_flag;
    bool enc_flag;
    hasp_status_t hasp_status;
    hasp_handle_t lic_handle;

    CLicense();

public:
    static const int LIC_OK = 0;
    static const int LIC_BROKEN = -1;
    static const int LIC_LOST = -2;
    static const int LIC_ATTACKED = -3;

    static CLicense &GetInstance();

    void * ServiceRoutine();

    /*��ʼ����Ȩ*/
    lic_result_t Start(int feature_id, int expacted_n_permissions);

    /*�˳���Ȩ���*/
    lic_result_t Exit();

    /*��ȡ��Ȩ״̬��ͬʱ���ؼ��ܹ�״̬*/
    lic_result_t Status(int *_hasp_status);

    /*��/�ر���Ȩ������Ϣ����̨���*/
    void SetDebugMode(bool mode);

    /*��ȡ��Ȩ������Ϣ*/
    const std::string &GetErrorMsg();

    /*��ȡ��ǰ��hasp���ܹ�״̬*/
    lic_result_t GetHaspStatus();

    /*��ȡ��Ȩ��*/
    int GetPermissionCount();

    void VendercodeEncDenc();
};



}}//end of name spaces

#endif