# llfcchat 客户端项目梳理教程

这是一份面向初学者的项目导读。目标是让你在 1 到 2 小时内看懂这个 Qt 聊天客户端的主流程，并且知道每个功能应该去哪里改。

## 1. 先知道这个项目在做什么

这是一个基于 Qt Widgets 的聊天客户端，整体流程是：

1. 启动程序，加载样式和配置。
2. 进入登录窗口。
3. 通过 HTTP 请求网关服务完成登录鉴权。
4. 登录成功后，建立 TCP 长连接进入聊天主界面。
5. 在主界面中处理搜索用户、加好友、好友申请、文本消息、心跳和下线通知。

核心上可以把它分成三层：

1. UI 层：窗口和自定义控件。
2. 业务层：用户数据管理、列表加载、页面切换。
3. 通信层：HTTP 管理器 + TCP 管理器。

## 2. 项目结构速览

建议先看这些文件：

1. 启动入口
   [main.cpp](main.cpp)
2. 主窗口与页面切换
   [mainwindow.h](mainwindow.h)
   [mainwindow.cpp](mainwindow.cpp)
3. 登录注册重置
   [logindialog.h](logindialog.h)
   [logindialog.cpp](logindialog.cpp)
   [registerdialog.cpp](registerdialog.cpp)
   [resetdialog.cpp](resetdialog.cpp)
4. 通信层
   [httpmgr.h](httpmgr.h)
   [httpmgr.cpp](httpmgr.cpp)
   [tcpmgr.h](tcpmgr.h)
   [tcpmgr.cpp](tcpmgr.cpp)
5. 聊天主界面
   [chatdialog.h](chatdialog.h)
   [chatdialog.cpp](chatdialog.cpp)
   [chatpage.h](chatpage.h)
   [chatpage.cpp](chatpage.cpp)
6. 用户数据模型
   [userdata.h](userdata.h)
   [usermgr.h](usermgr.h)
   [usermgr.cpp](usermgr.cpp)
7. 公共协议与配置
   [global.h](global.h)
   [global.cpp](global.cpp)
   [config.ini](config.ini)
   [llfcchat.pro](llfcchat.pro)

## 3. 从启动到进入聊天界面的完整链路

### 第一步：程序启动

[main.cpp](main.cpp) 做了三件事：

1. 加载 QSS 样式资源。
2. 从程序目录读取 config.ini，拼接网关地址 gate_url_prefix。
3. 创建并显示 MainWindow。

### 第二步：MainWindow 负责页面切换

[mainwindow.cpp](mainwindow.cpp) 中把 LoginDialog、RegisterDialog、ResetDialog、ChatDialog 当成中心窗口进行切换。

关键槽函数：

1. SlotSwitchReg：登录页切到注册页。
2. SlotSwitchReset：登录页切到重置页。
3. SlotSwitchChat：收到 TCP 登录成功信号后切到聊天页。
4. SlotOffline / SlotExcepConOffline：处理异地登录或连接异常，回到登录页。

### 第三步：登录先 HTTP，再 TCP

[logindialog.cpp](logindialog.cpp) 的核心流程：

1. 点击登录按钮，先校验邮箱和密码。
2. 调用 HttpMgr::PostHttpReq 请求 user_login。
3. 登录回包中取出 uid、host、port、token。
4. 发信号给 TcpMgr 建立长连接。
5. TCP 连接成功后发 ID_CHAT_LOGIN 请求。
6. TcpMgr 收到 ID_CHAT_LOGIN_RSP 后把用户和好友数据写入 UserMgr。
7. 发 sig_swich_chatdlg 通知 MainWindow 切到 ChatDialog。

这条链路是全项目最重要的一条。

## 4. HTTP 与 TCP 通信层怎么分工

### HTTP 管理器

[httpmgr.cpp](httpmgr.cpp) 主要负责：

1. 发 POST JSON 请求。
2. 收到响应后统一转发为 sig_http_finish。
3. 再按模块分发到注册、重置、登录三个信号。

模块枚举定义在 [global.h](global.h) 的 Modules。

### TCP 管理器

[tcpmgr.cpp](tcpmgr.cpp) 负责：

1. 建立和维护 QTcpSocket。
2. 按协议读取消息头和消息体。
3. 根据 ReqId 分发到不同 handler。
4. 把业务事件用 Qt signal 抛给 UI 层。

消息类型枚举在 [global.h](global.h) 的 ReqId。

重点消息：

1. ID_CHAT_LOGIN_RSP：登录聊天服成功。
2. ID_SEARCH_USER_RSP：搜索用户。
3. ID_NOTIFY_ADD_FRIEND_REQ：收到好友申请。
4. ID_AUTH_FRIEND_RSP：好友认证结果。
5. ID_NOTIFY_TEXT_CHAT_MSG_REQ：收到文本消息。
6. ID_HEARTBEAT_RSP：心跳响应。
7. ID_NOTIFY_OFF_LINE_REQ：异地登录下线通知。

## 5. ChatDialog 是怎么组织页面的

[chatdialog.cpp](chatdialog.cpp) 可以理解为聊天场景的总控。

### 左侧导航

1. 聊天
2. 联系人
3. 设置

通过 side_xxx 点击切换 stackedWidget 页面。

### 三种列表与搜索

1. 聊天列表：chat_user_list。
2. 联系人列表：con_user_list。
3. 搜索列表：search_list。

ShowSearch 用来统一切换可见状态。

### 心跳

构造函数中启动 QTimer，每 10 秒发送一次 ID_HEART_BEAT_REQ。

### 消息更新

1. 收到对端文本消息：slot_text_chat_msg。
2. 自己发送文本消息后更新列表摘要：slot_append_send_chat_msg。
3. 当前聊天窗口追加内容：UpdateChatMsg 和 ChatPage::AppendChatMsg。

## 6. 搜索、加好友、联系人链路

### 搜索用户

[searchlist.cpp](searchlist.cpp) 中点击“添加用户”条目会发 ID_SEARCH_USER_REQ。

返回后有两种情况：

1. 已经是好友：直接发信号跳转到聊天项。
2. 不是好友：弹出查找成功对话框并可申请好友。

### 好友申请

1. TCP 收到 ID_NOTIFY_ADD_FRIEND_REQ。
2. ChatDialog::slot_apply_friend 更新红点、列表和申请页。
3. 申请页逻辑在 [applyfriendpage.cpp](applyfriendpage.cpp)。

### 好友认证后刷新

当认证成功：

1. UserMgr 新增好友。
2. 联系人列表新增项。
3. 聊天列表新增项。

对应处理分布在 [chatdialog.cpp](chatdialog.cpp) 和 [contactuserlist.cpp](contactuserlist.cpp)。

## 7. UserMgr 和数据模型怎么理解

### 数据结构

[userdata.h](userdata.h) 定义了主要结构：

1. UserInfo：当前用户或界面展示用户。
2. FriendInfo：好友与聊天摘要。
3. ApplyInfo：好友申请。
4. TextChatData / TextChatMsg：聊天消息。

### 状态管理

[usermgr.cpp](usermgr.cpp) 保存并维护：

1. 当前用户信息。
2. 申请列表。
3. 好友列表和好友映射表。
4. 聊天/联系人分页加载下标。
5. 指定好友的聊天记录追加。

你可以把它看成客户端内存态的小型仓库。

## 8. 按顺序读代码的建议路线

如果你第一次接触这个项目，推荐顺序：

1. [main.cpp](main.cpp)
2. [mainwindow.cpp](mainwindow.cpp)
3. [logindialog.cpp](logindialog.cpp)
4. [httpmgr.cpp](httpmgr.cpp)
5. [tcpmgr.cpp](tcpmgr.cpp)
6. [usermgr.cpp](usermgr.cpp)
7. [chatdialog.cpp](chatdialog.cpp)
8. [chatpage.cpp](chatpage.cpp)
9. [searchlist.cpp](searchlist.cpp)
10. [applyfriendpage.cpp](applyfriendpage.cpp)
11. [contactuserlist.cpp](contactuserlist.cpp)

按这个顺序读，能从“入口与主流程”逐步过渡到“细节组件”。

## 9. 如何运行与调试

### qmake 工程配置

项目文件是 [llfcchat.pro](llfcchat.pro)，使用 qmake。

里面已经声明：

1. QT += core gui network
2. Qt5/Qt6 下启用 widgets
3. 构建后复制 config.ini 和 static 目录到输出目录

### 运行前确认

1. config.ini 中网关地址是否可用。
2. 服务器是否已经启动并监听配置端口。
3. Qt kit 与编译器是否匹配。

### 调试建议

1. 登录问题优先看 HttpMgr 和 LoginDialog。
2. 连接问题优先看 TcpMgr 的 connected/error/disconnected。
3. 消息不显示优先看 ChatDialog::slot_text_chat_msg 和 ChatPage::AppendChatMsg。
4. 好友链路问题优先看 UserMgr 映射表是否更新。

## 10. 这个项目里需要特别注意的点

1. 有一些“模拟数据”代码仍然保留，阅读时要区分真实后端数据和测试填充数据。
2. UI 与网络通过信号槽强耦合，排错时要沿着 signal 到 slot 追踪。
3. ReqId 是通信协议核心，新增消息一定要同时改 global.h、TcpMgr handler 和 UI 槽函数。
4. gate_url_prefix 来自运行目录下 config.ini，不是源码目录。

## 11. 下一步你可以怎么做

你已经有了全局视图。建议现在做两件小事强化理解：

1. 打断点走一遍登录成功到进入聊天页。
2. 手工触发一次搜索用户和发送文本消息，观察 ReqId 流转。

完成这两步后，你基本就能独立改功能了。
