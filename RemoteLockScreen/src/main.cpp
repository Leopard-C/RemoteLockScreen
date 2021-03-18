/****************************************************************************
 *
 *      Author: github@Leopard-C
 * Description: 通过HTTP请求远程锁屏
 *              同时与“RemoteLockScreenProtector_Releasex64.exe”互相守护
 *              两者中任何一个退出，都会触发锁屏
 *      Update: 2021-03-17
 *
****************************************************************************/
#include "httplib.h"
#include "util.h"
#include <fstream>
#include <sstream>
#include <atomic>

// release模式下，隐藏控制台
#ifndef _DEBUG
#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
#endif


void setPlainContent(httplib::Response& res, const std::string& content);
void setHtmlContent(httplib::Response& res, const std::string& content);
void setHtmlContentEx(httplib::Response& res, const std::string& content);

bool checkPassword(const httplib::Request& req, httplib::Response& res);
bool checkLocalhost(const httplib::Request& req, httplib::Response& res);

static const std::string s_index_html = "D:/etc/remote_lock_screen/index.html";
static const std::string s_auth_html  = "D:/etc/remote_lock_screen/auth.html";
static const std::string s_password   = "your custom password";

static bool s_should_quit = false;
static std::atomic_int s_threads_count = 0;


/***************************************************************
 *
 *         守护 “远程锁屏守护进程” (禁止套娃）
 *         与Protector互相守护，任何一个一个被终止，立即锁屏
 *
***************************************************************/
static const char* s_remote_lock_screen_exe_name = "RemoteLockScreen_Releasex64.exe";
static const char* s_remote_lock_screen_protector_exe_name = "RemoteLockScreenProtector_Releasex64.exe";
static unsigned long s_remote_lock_screen_protector_exe_pid = 0;
void ProtectRemoteLockScreenProtector() {
    while (!s_should_quit) {
        if (s_remote_lock_screen_protector_exe_pid > 0) {
            if (!util::is_process_running(s_remote_lock_screen_protector_exe_pid)) {
                ::LockWorkStation();
                s_remote_lock_screen_protector_exe_pid = 0;
            }
            // is_process_running执行速度超级快，间隔1ms, 可以更短(sleep_us)
            // 经过测试，间隔1ms，即使使用taskkill命令，在一个bat脚本中，“同时”杀这两个进程，也会触发锁屏
            util::sleep_ms(1);
        }
        else {
            unsigned long pid = util::get_pid(s_remote_lock_screen_protector_exe_name);
            if (pid > 0) {
                s_remote_lock_screen_protector_exe_pid = pid;
            }
            // get_pid比较耗时，间隔1s，只要两个进程都在运行，是不会执行到这里的
            for (int i = 0; i < 100 && !s_should_quit; ++i) {
                util::sleep_ms(10);
            }
        }
    }
    s_threads_count.fetch_sub(1); // 总线程数减1
}


/***************************************************************
 *
 *        取消注释下面一行，表示启用 拍照功能（需要opencv库支持）
 *
***************************************************************/
#define TAKE_PHOTO
#ifdef TAKE_PHOTO
#include <opencv2/opencv.hpp>
static const std::string s_photo_path = "G:/RLS/";
bool TakePhoto() {
    std::string filename = s_photo_path + std::to_string(time(NULL)) + ".jpg";
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        ::MessageBox(NULL, "RLS error 1", "Error", 1);
        return false;
    }
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 1080);
    cv::Mat frame;
    cap >> frame;
    if (!cv::imwrite(filename, frame)) {
        ::MessageBox(NULL, "RLS error 2", "Error", 1);
        return false;
    }
    return true;
}
#endif


/***************************************************************
 *
 *         取消注释下面一行，表示启用 断网自动锁屏
 *
***************************************************************/
#define AUTO_LOCK_WHEN_NET_DISCONNECT
#ifdef AUTO_LOCK_WHEN_NET_DISCONNECT
static bool s_is_net_ok = false; // 网络状态
// 检测网络状态
bool checkBaidu() {
    httplib::Client client("http://www.baidu.com");
    client.set_connection_timeout(1);
    client.set_read_timeout(1);
    client.set_write_timeout(1);
    httplib::Result res = client.Get("/");
    return res && res->status == 200 && res->body.find("STATUS OK") != std::string::npos;
}
bool checkHttpbin() {
    httplib::Client client("http://httpbin.org");
    client.set_connection_timeout(1);
    client.set_read_timeout(1);
    client.set_write_timeout(1);
    httplib::Result res = client.Get("/ip");
    return res && res->status == 200 && res->body.find("\"origin\"") != std::string::npos;
}
// 多检测几次，更精确一些，相应的，从真正断网，到锁屏的时间可能会延长（四五秒）
// 避免网络抖动误锁屏
bool isNetworkOk() {
    for (int i = 0; i < 3; ++i) {
        if (checkBaidu() || checkHttpbin()) {
            return true;
        }
        util::sleep_ms(300);
    }
    return false;
}
void AutoLockWhenNetDisconnect() {
    while (!s_should_quit) {
        if (isNetworkOk()) {
            s_is_net_ok = true;
        }
        else {
            if (s_is_net_ok) {
                util::lock_screen(30);
            }
            s_is_net_ok = false;
        }
        for (int i = 0; i < 100 && !s_should_quit; ++i) { // 间隔2s
            util::sleep_ms(20);
        }
    }
    s_threads_count.fetch_sub(1);  // 总线程数减去1
}
#endif


/*
 *  只能启动一个进程
 */
bool start() {
    HANDLE hMutex = CreateMutex(NULL, FALSE, s_remote_lock_screen_exe_name);
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

    // 守护进程退出，立即锁屏
    s_threads_count.fetch_add(1);
    std::thread t0(ProtectRemoteLockScreenProtector);
    t0.detach();

#ifdef AUTO_LOCK_WHEN_NET_DISCONNECT
    // 断网锁屏
    s_threads_count.fetch_add(1);
    std::thread t1(AutoLockWhenNetDisconnect);
    t1.detach();
#endif

    // 服务器
    httplib::Server svr;

    // 主页，GET，无需密码
    svr.Get("/", [&svr](const httplib::Request& req, httplib::Response& res) {
        std::ifstream ifs(s_auth_html);
        if (!ifs) {
            return setHtmlContentEx(res, "404 Not Found");
        }
        std::stringstream ss;
        ss << ifs.rdbuf();
        setHtmlContent(res, ss.str());
    });

    // 主页，POST，必须携带密码
    svr.Post("/", [&svr](const httplib::Request& req, httplib::Response& res) {
        if (checkPassword(req, res)) {
            std::ifstream ifs(s_index_html);
            if (!ifs) {
                return setHtmlContentEx(res, "404 Not Found");
            }
            std::stringstream ss;
            ss << ifs.rdbuf();
            setHtmlContent(res, ss.str());
        }
    });

    // 检验密码
    svr.Post("/check_password", [&svr](const httplib::Request& req, httplib::Response& res) {
        if (checkPassword(req, res)) {
            setPlainContent(res, "OK");
        }
    });

    // 锁屏
    svr.Post("/lock", [](const httplib::Request& req, httplib::Response& res) {
        if (checkPassword(req, res)) {
#ifdef TAKE_PHOTO
            std::thread t(TakePhoto); // 拍张照片, 启动新的线程，防止阻塞在这里，子线程拍完照就会退出
            t.detach();
#endif
            if (util::is_screen_locked()) {
                setPlainContent(res, "Already Locked");
            }
            else {
                if (util::lock_screen(10)) {
                    setPlainContent(res, "Locked");
                }
                else {
                    setPlainContent(res, "Interval at least 10s");
                }
            }
        }
    });

    // 检查是否锁屏
    //   锁屏   ==> 1
    //   未锁屏 ==> 0
    svr.Post("/is_locked", [](const httplib::Request& req, httplib::Response& res) {
        if (checkPassword(req, res)) {
            if (util::is_screen_locked()) {
                setPlainContent(res, "1");
            }
            else {
                setPlainContent(res, "0");
            }
        }
    });

    // 停止服务器
    // 只能从本机发出该请求！！！
    svr.Get("/stop", [&svr](const httplib::Request& req, httplib::Response& res) {
        if (checkLocalhost(req, res) && checkPassword(req, res)) {
            svr.stop();
            setPlainContent(res, "Stopped");
        }
    });

    MessageBoxTimeout(NULL, "RemoteLockScreen started!", "Prompt", MB_ICONINFORMATION, GetSystemDefaultLangID(), 3000);

    // 启动服务器
    // 正常启动后，阻塞，直到调用svr.stop();
    svr.listen("0.0.0.0", 8080);

    // 等待所有子线程退出
    // 最多等待2s
    s_should_quit = true;
    for (int i = 0; i < 100 && s_threads_count > 0; ++i) {
        util::sleep_ms(20);
    }
}

/*
 *  认证请求密码
 */
bool checkPassword(const httplib::Request& req, httplib::Response& res) {
    std::string content;
    if (req.has_param("pwd")) {
        std::string pwd = req.get_param_value("pwd");
        if (pwd == s_password) {
            return true;
        }
        else {
            content = "Wrong password";
        }
    }
    else {
        content = "Missing password";
    }
    setPlainContent(res, content);
    return false;
}


/*
 *  检查请求是否从本机发出
 */
bool isFromLocalhost(const httplib::Request& req) {
    return req.remote_addr == "127.0.0.1";
}
bool checkLocalhost(const httplib::Request& req, httplib::Response& res) {
    if (isFromLocalhost(req)) {
        return true;
    }
    setPlainContent(res, "Invalid Request");
    return false;
}

/*
 *   返回指定格式的内容
 */
void setPlainContent(httplib::Response& res, const std::string& content) {
    res.set_content(content, "text/plain");
}

void setHtmlContent(httplib::Response& res, const std::string& content) {
    res.set_content(content, "text/html;charset=utf-8");
}

void setHtmlContentEx(httplib::Response& res, const std::string& content) {
    setHtmlContent(res, "<html><body><h1>" + content + "</h1></body></html>");
}

