#pragma once
#include <windows.h>
#include <winuser.h>

#pragma warning(disable:4996)

// MessageBox超时自动关闭
// 仍然阻塞
// 单位：ms
extern "C" {
    int WINAPI MessageBoxTimeoutA(IN HWND hWnd, IN LPCSTR lpText, IN LPCSTR lpCaption, IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);
    int WINAPI MessageBoxTimeoutW(IN HWND hWnd, IN LPCWSTR lpText, IN LPCWSTR lpCaption, IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);
}
#ifdef UNICODE
#define MessageBoxTimeout MessageBoxTimeoutW
#else
#define MessageBoxTimeout MessageBoxTimeoutA
#endif


namespace util {

    bool is_screen_locked();

    bool lock_screen(int min_interval = 0);

    unsigned long get_pid(const char *procressName);

    bool is_process_running(unsigned long pid);

    void sleep_ms(unsigned int ms);

    void sleep_us(unsigned int us);

} // namespace util
