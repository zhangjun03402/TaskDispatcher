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

    /*初始化授权*/
    lic_result_t Start(int feature_id, int expacted_n_permissions);

    /*退出授权监控*/
    lic_result_t Exit();

    /*获取授权状态，同时返回加密狗状态*/
    lic_result_t Status(int *_hasp_status);

    /*打开/关闭授权调试信息控制台输出*/
    void SetDebugMode(bool mode);

    /*获取授权错误信息*/
    const std::string &GetErrorMsg();

    /*获取当前的hasp加密狗状态*/
    lic_result_t GetHaspStatus();

    /*获取授权数*/
    int GetPermissionCount();

    void VendercodeEncDenc();
};



}}//end of name spaces

#endif