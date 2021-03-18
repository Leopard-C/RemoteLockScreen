#include "util.h"
#include <atlstr.h>
#include <tlhelp32.h>
#include <wtsApi32.h>
#include <mutex>

namespace util {

/*
 *  获取指定进程名的PID
 */
unsigned long get_pid(const char *procressName) {
    char pName[MAX_PATH];
    strcpy(pName, procressName);
    CharLowerBuff(pName, MAX_PATH);
    PROCESSENTRY32 currentProcess;                        //存放快照进程信息的一个结构体
    currentProcess.dwSize = sizeof(currentProcess);        //在使用这个结构之前，先设置它的大小
    HANDLE hProcess = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);  //给系统内的所有进程拍一个快照

    if (hProcess == INVALID_HANDLE_VALUE) {
        return 0;
    }

    bool bMore = Process32First(hProcess, &currentProcess);        //获取第一个进程信息
    while(bMore) {
        CharLowerBuff(currentProcess.szExeFile,MAX_PATH);
        if (strcmp(currentProcess.szExeFile, pName) == 0) {
            CloseHandle(hProcess);
            return currentProcess.th32ProcessID;
        }
        bMore = Process32Next(hProcess, &currentProcess);            //遍历下一个
    }

    CloseHandle(hProcess);    //清除hProcess句柄
    return 0;
}

/*
 *  判断指定PID的进程是否存在
 */
bool is_process_running(unsigned long pid) {
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    DWORD ret = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return ret == WAIT_TIMEOUT;
}


/*
 *  锁屏 （最小间隔）
 */
static time_t s_last_lock_time = 1;
static std::mutex s_mutex_for_lock_screen;
bool lock_screen(int min_interval/* = 0*/) {
    std::lock_guard<std::mutex> lck(s_mutex_for_lock_screen);
    time_t now = time(NULL);
    if (now - s_last_lock_time < min_interval) {
        return false;
    }
    s_last_lock_time = now;
    ::LockWorkStation();
    return true;
}


/*
 *  判断是否已经锁屏
 */
bool is_screen_locked() {
    typedef BOOL(PASCAL * WTSQuerySessionInformation)(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPTSTR* ppBuffer, DWORD* pBytesReturned);
    typedef void (PASCAL * WTSFreeMemory)(PVOID pMemory);

    WTSINFOEXW * pInfo = NULL;
    WTS_INFO_CLASS wtsic = WTSSessionInfoEx;
    bool bRet = false;
    LPTSTR ppBuffer = NULL;
    DWORD dwBytesReturned = 0;
    LONG dwFlags = 0;
    WTSQuerySessionInformation pWTSQuerySessionInformation = NULL;
    WTSFreeMemory pWTSFreeMemory = NULL;

    HMODULE hLib = LoadLibrary("wtsapi32.dll");
    if (!hLib) {
        return false;
    }
    pWTSQuerySessionInformation = (WTSQuerySessionInformation)GetProcAddress(hLib, "WTSQuerySessionInformationW");
    if (pWTSQuerySessionInformation) {
        pWTSFreeMemory = (WTSFreeMemory)GetProcAddress(hLib, "WTSFreeMemory");
        if (pWTSFreeMemory != NULL) {
            DWORD dwSessionID = WTSGetActiveConsoleSessionId();
            if (pWTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, dwSessionID, wtsic, &ppBuffer, &dwBytesReturned)) {
                if (dwBytesReturned > 0) {
                    pInfo = (WTSINFOEXW*)ppBuffer;
                    if (pInfo->Level == 1) {
                        dwFlags = pInfo->Data.WTSInfoExLevel1.SessionFlags;
                    }
                    if (dwFlags == WTS_SESSIONSTATE_LOCK) {
                        bRet = true;
                    }
                }
                pWTSFreeMemory(ppBuffer);
                ppBuffer = NULL;
            }
        }
    }
    if (hLib != NULL) {
        FreeLibrary(hLib);
    }
    return bRet;
}

/*
 *  当前线程暂停ms毫秒
 */
void sleep_ms(unsigned int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/*
 *  当前线程暂停us微秒
 */
void sleep_us(unsigned int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

} // namespace util

