/*
 * README.md - AVServer 音视频服务器项目快速开始指南
 *
 * 欢迎使用AVServer，这是一个完整的、为初学者设计的Linux C++音视频服务器实现！
 */

# AVServer - 音视频服务器项目

## 项目简介

AVServer是一个**完整的Linux服务端C++音视频流服务器项目**，专为初学者设计。
项目包含详细的代码注释和学习指南，涵盖多线程、TCP网络编程、消息协议等核心概念。

### 核心特性

✅ **完整的功能实现**
- TCP服务器框架，支持多客户端并发连接
- 线程池，用于高效处理客户端请求
- 自定义消息协议，用于音视频数据传输
- 帧缓冲池，优化实时性能
- 编解码器接口，支持多种格式

✅ **学习友好的代码**
- 详细的中文注释（占代码的40%）
- 清晰的模块化架构
- 15+个使用示例
- 生产级别的代码质量

✅ **现代C++标准**
- C++17标准，展示最佳实践
- 智能指针、完美转发、模板元编程
- RAII原则，自动资源管理

---

## 项目文件清单

```
AVServer_01_SafeQueue.h              (300行) - 线程安全队列
AVServer_02_CircularBuffer.h         (320行) - 循环缓冲区
AVServer_03_FrameBuffer.h            (350行) - 帧缓冲和缓冲池
AVServer_04_ThreadPool.h             (300行) - 线程池实现
AVServer_05_CodecInterfaces.h        (250行) - 编解码接口
AVServer_06_MessageProtocol.h        (450行) - TCP消息协议
AVServer_07_TcpServer.h              (400行) - TCP服务器
AVServer_08_Connection.h             (400行) - 客户端连接管理
AVServer_09_AVServer.h               (400行) - 应用程序主类
AVServer_10_main.cpp                 (450行) - 程序入口点
AVServer_项目规划.md                  - 项目整体规划文档
AVServer_项目总结.md                  - 详细的项目总结
```

**总计：约3500行代码和详细注释**

---

## 快速开始

### 1. 编译项目

**Linux/Unix (GCC)：**
```bash
cd /path/to/avserver
g++ -std=c++17 -pthread -O2 -o avserver AVServer_10_main.cpp
```

**macOS (Clang)：**
```bash
clang++ -std=c++17 -pthread -O2 -o avserver AVServer_10_main.cpp
```

**Windows (Visual Studio)：**
- 创建新的C++控制台项目
- 添加所有.h和.cpp文件到项目中
- 确保C++标准设置为C++17或更新
- 编译即可（自动链接ws2_32.lib）

### 2. 运行服务器

```bash
# 使用默认配置（端口8888）
./avserver

# 指定自定义端口
./avserver 9999

# 或使用--port参数
./avserver --port 9999
```

### 3. 测试连接

**使用netcat连接服务器：**
```bash
nc localhost 8888
```

**使用telnet连接：**
```bash
telnet localhost 8888
```

### 4. 交互式命令

启动服务器后，可以使用以下命令：

| 命令 | 说明 |
|------|------|
| `help` | 显示帮助信息 |
| `status` | 显示服务器运行状态 |
| `stats` | 显示详细的统计信息 |
| `conns` | 显示当前连接数 |
| `quit` | 优雅关闭服务器 |
| `exit` | 同上 |

---

## 学习路线图

### 第1周：基础数据结构
1. 学习 `AVServer_01_SafeQueue.h`
   - 理解线程同步机制
   - 学习std::mutex和std::condition_variable
   - 运行示例代码，观察阻塞和通知机制

2. 学习 `AVServer_02_CircularBuffer.h`
   - 理解环形缓冲区的工作原理
   - 学习write/read/peek操作
   - 理解wrap-around处理

### 第2周：并发编程
3. 学习 `AVServer_04_ThreadPool.h`
   - 理解线程池的设计和实现
   - 学习add_task()和add_work()的区别
   - 理解优雅关闭机制

4. 学习 `AVServer_03_FrameBuffer.h`
   - 理解对象池设计模式
   - 学习资源复用和性能优化
   - 理解统计信息跟踪

### 第3周：网络编程
5. 学习 `AVServer_06_MessageProtocol.h`
   - 理解自定义协议设计
   - 学习消息序列化和反序列化
   - 理解CRC校验机制

6. 学习 `AVServer_07_TcpServer.h`
   - 理解TCP服务器框架
   - 学习accept和socket操作
   - 理解事件回调机制

### 第4周：应用集成
7. 学习 `AVServer_08_Connection.h`
   - 理解单客户端连接管理
   - 学习消息提取和处理
   - 理解连接生命周期

8. 学习 `AVServer_09_AVServer.h`
   - 理解模块集成
   - 学习事件驱动设计
   - 理解统计和监控

### 第5周：实战项目
9. 学习 `AVServer_10_main.cpp`
   - 理解应用程序架构
   - 学习命令行参数处理
   - 理解信号处理和优雅关闭

10. 编译运行、测试和优化
    - 编译整个项目
    - 使用多个客户端测试
    - 观察性能指标
    - 尝试功能扩展

---

## 模块详解

### 模块1：线程安全队列 (SafeQueue)
**用途**：在多个线程间安全地传递任务和数据

```cpp
SafeQueue<Message> queue;

// 生产者线程
queue.push(message);

// 消费者线程
Message msg;
if (queue.pop(msg)) {  // 阻塞等待消息
    process(msg);
}
```

**关键概念**：
- std::mutex：互斥锁
- std::condition_variable：条件变量
- lock_guard vs unique_lock的区别

---

### 模块2：循环缓冲区 (CircularBuffer)
**用途**：用于实时数据流的高效缓冲

```cpp
CircularBuffer buffer(1024);

// 写入数据
buffer.write(data, size);

// 读取数据
buffer.read(output, size);

// 查看数据（不消费）
buffer.peek(view, size);
```

**关键概念**：
- 环形缓冲区原理
- write_pos和read_pos的管理
- wrap-around处理

---

### 模块3：线程池 (ThreadPool)
**用途**：管理工作线程，分发任务

```cpp
ThreadPool pool(4);  // 4个工作线程

// 异步执行，返回future
auto future = pool.add_task([]() { return 42; });

// 获取结果
int result = future.get();

// fire-and-forget任务
pool.add_work([]() { std::cout << "Task" << std::endl; });
```

**关键概念**：
- 线程池的工作原理
- std::future和std::packaged_task
- 完美转发（std::forward）

---

### 模块4：TCP服务器 (TcpServer)
**用途**：接受客户端连接并分发处理

```cpp
ServerConfig config;
config.port = 8888;

TcpServer server(config);
server.start();

// 设置回调
server.set_on_client_connected([](auto conn) {
    std::cout << "Client: " << conn->get_addr() << std::endl;
});

// 广播消息
server.broadcast(message);
```

**关键概念**：
- TCP套接字编程
- listen和accept操作
- 事件回调设计

---

### 模块5：消息协议 (MessageProtocol)
**用途**：定义和处理网络消息的格式

```cpp
// 创建消息
Message msg(MessageType::VIDEO_FRAME, 1024);
msg.set_payload(data, 1024);

// 序列化
auto bytes = msg.to_bytes();

// 反序列化
Message received;
received.from_bytes(bytes);
```

**消息格式**：
```
[魔数:4B] [类型:2B] [大小:4B] [时间戳:8B] [CRC:2B] [消息体]
```

---

### 模块6：应用程序 (AVServer)
**用途**：整合所有模块，提供统一的应用程序接口

```cpp
AVServer server(config);
server.start();

// 获取统计信息
auto stats = server.get_statistics();
std::cout << stats.to_string() << std::endl;

// 优雅关闭
server.stop();
```

---

## 核心概念学习

### 并发编程
- **互斥锁 (Mutex)**：保护共享资源
- **条件变量 (Condition Variable)**：线程间的信号传递
- **原子操作 (Atomic)**：无锁操作，提高性能
- **RAII**：构造时获取资源，析构时释放

### 网络编程
- **TCP套接字**：可靠的字节流连接
- **accept和listen**：服务器接受连接
- **send和recv**：数据发送和接收
- **缓冲区管理**：处理粘包问题

### 消息处理
- **序列化**：对象转为字节流
- **反序列化**：字节流转为对象
- **消息格式**：定义统一的协议格式
- **校验机制**：CRC保证数据完整性

### 设计模式
- **线程池**：资源管理和任务分发
- **对象池**：复用对象，减少分配
- **观察者（回调）**：事件驱动设计
- **生产者-消费者**：线程间的数据传递

---

## 常见问题

**Q: 代码会在我的操作系统上运行吗？**

A: 支持Linux和macOS（使用POSIX套接字）。Windows需要将socket调用改为Winsock2 API，代码中已有ifdef适配。

**Q: 如何实现实际的视频编码？**

A: 集成FFmpeg库。将`AVServer_05_CodecInterfaces.h`中的虚拟方法改为FFmpeg调用。参考FFmpeg文档。

**Q: 如何支持更多并发连接？**

A: 1) 增加ThreadPool的线程数；2) 使用epoll代替select；3) 优化内存使用；4) 考虑异步I/O。

**Q: 如何添加新的消息类型？**

A: 1) 在MessageType枚举中添加；2) 在ProtocolHelper中添加字符串转换；3) 在AVServer中添加处理函数。

**Q: 如何监控服务器性能？**

A: 调用`get_statistics()`获取统计信息，包括连接数、消息数、字节数、帧率、码率等。

---

## 推荐扩展

### 短期扩展（1-2周）
1. 添加日志系统（spdlog库）
2. 添加配置文件支持
3. 实现简单的心跳机制
4. 添加性能指标导出

### 中期扩展（2-4周）
1. 集成FFmpeg进行实际编解码
2. 支持多种消息类型
3. 实现自适应码率算法
4. 添加客户端SDK

### 长期扩展（4周以上）
1. 支持RTMP/RTSP协议
2. 高性能优化（epoll, 零拷贝）
3. 集群部署支持
4. 实时流统计和分析

---

## 文件结构

```
.
├── AVServer_01_SafeQueue.h          # 线程安全队列
├── AVServer_02_CircularBuffer.h     # 循环缓冲区
├── AVServer_03_FrameBuffer.h        # 帧缓冲管理
├── AVServer_04_ThreadPool.h         # 线程池
├── AVServer_05_CodecInterfaces.h    # 编解码接口
├── AVServer_06_MessageProtocol.h    # 消息协议
├── AVServer_07_TcpServer.h          # TCP服务器
├── AVServer_08_Connection.h         # 客户端连接
├── AVServer_09_AVServer.h           # 应用程序主类
├── AVServer_10_main.cpp             # 程序入口
├── AVServer_项目规划.md              # 项目规划
└── AVServer_项目总结.md              # 项目总结
```

---

## 编译命令参考

```bash
# 基本编译
g++ -std=c++17 -pthread -o avserver AVServer_10_main.cpp

# 优化编译
g++ -std=c++17 -pthread -O2 -o avserver AVServer_10_main.cpp

# 调试编译
g++ -std=c++17 -pthread -g -O0 -o avserver AVServer_10_main.cpp

# 所有警告
g++ -std=c++17 -pthread -Wall -Wextra -o avserver AVServer_10_main.cpp

# 使用clang
clang++ -std=c++17 -pthread -O2 -o avserver AVServer_10_main.cpp

# 静态链接
g++ -std=c++17 -pthread -static -O2 -o avserver AVServer_10_main.cpp
```

---

## 运行示例

```bash
# 启动服务器
$ ./avserver
=== AVServer - Audio/Video Server ===
Version: 1.0
Built with: C++17

[CONFIG] Server Configuration:
  Listen Address: 0.0.0.0
  Listen Port: 8888
  Max Connections: 1000
  ...

[STARTUP] Server started successfully
[STARTUP] Listening on 0.0.0.0:8888

> help
=== AVServer Commands ===
help       - Show this help message
status     - Show server status
stats      - Show server statistics
conns      - Show current connection count
quit/exit  - Shutdown server gracefully

> status
[STATUS] Server is RUNNING

> stats
=== Server Statistics ===
Uptime: 120s
Current Connections: 3
Total Connections: 5
...

> quit
[QUIT] Shutting down server...
```

---

## 许可证

本项目为教学目的创建。代码质量达到生产级别，但在实际部署前应进行充分测试和优化。

---

## 致谢

感谢所有为C++和网络编程做出贡献的开发者和教育工作者。
本项目旨在帮助初学者理解复杂的系统编程概念。

---

## 支持和反馈

如有任何问题或建议，欢迎通过以下方式反馈：
- 查看代码中的详细注释
- 阅读项目总结文档
- 参考标准C++文档
- 查阅网络编程资料

---

**祝你学习愉快！** 🚀

---

最后更新：2025年
版本：1.0
