## Remote Lock Screen

两个程序

+ RemoteLockScreen：启动一个服务器，接受指定HTTP请求，锁住电脑屏幕。
+ RemoteLockScreenProtector：守护RemoteLockScreen，同时也被守护。（互相守护），二者中任何一个退出，都会触发锁屏。

使用的C++ HTTP库：[yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib)

自行修改两个`main.cpp`中的 `文件路径`、`程序名` 等自定义的全局静态参数。如

+ `s_index_html`
+ `s_auth_html`
+ `s_password`
+ `s_remote_lock_screen_exe_name`
+ `s_remote_lock_screen_protector_exe_name`
+ `s_photo_path`

其实可以写个配置文件读取的功能，将这些参数放在配置文件中（懒）。


如果电脑IP不固定，或者想通过公网访问电脑，可以考虑`frp`内网穿透。

开机启动这两个程序，请使用windows自带的`计划任务`(`task scheduler`)，最好延时1分钟执行，这样万一程序出了点什么问题，可以在这一分钟内，阻止该程序的运行。
