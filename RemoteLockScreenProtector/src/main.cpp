/***********************************************************************
 *
 *      Author: github@Leopard-C
 * Description: 与“RemoteLockScreen_Releasex64.exe”互相守护
 *              两者中任何一个退出，都会触发锁屏
 *      Update: 2021-03-17
 *
************************************************************************/
#include "util.h"

// 隐藏控制台
#ifndef _DEBUG
#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
#endif

/*
 *  守护远程锁屏进程
 */
static const char* s_remote_lock_screen_exe_name = "RemoteLockScreen_Releasex64.exe";
static const char* s_remote_lock_screen_protector_exe_name = "RemoteLockScreenProtector_Releasex64.exe";
static unsigned long s_remote_lock_screen_exe_pid = 0;
void protectRemoteLockScreen() {
    while (true) {
        if (s_remote_lock_screen_exe_pid > 0) {
            if (!util::is_process_running(s_remote_lock_screen_exe_pid)) {
                ::LockWorkStation();
                s_remote_lock_screen_exe_pid = 0;
            }
            util::sleep_ms(1);
        }
        else {
            unsigned long pid = util::get_pid(s_remote_lock_screen_exe_name);
            if (pid > 0) {
                s_remote_lock_screen_exe_pid = pid;
            }
            util::sleep_ms(2000);
        }
    }
}

/*
 *  只能启动一个进程
 */
bool start() {
    HANDLE hMutex = CreateMutex(NULL, FALSE, s_remote_lock_screen_protector_exe_name);
    if (hMutex && (GetLastError() == ERROR_ALREADY_EXISTS)) {
        CloseHandle(hMutex);
        hMutex = NULL;
        return false;
    }
    return true;
}


int main() {
    if (!start()) {
        MessageBoxTimeout(NULL, "Already Started", "Prompt", MB_ICONINFORMATION, GetSystemDefaultLangID(), 3000);
        return 1;
    }
    MessageBoxTimeout(NULL, "RemoteLockScreenProtector start!", "Prompt", MB_ICONINFORMATION, GetSystemDefaultLangID(), 3000);
    protectRemoteLockScreen();
    return 0;
}

