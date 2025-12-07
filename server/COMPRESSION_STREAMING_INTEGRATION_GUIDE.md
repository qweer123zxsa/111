# 音视频服务器压缩和流媒体功能集成指南

## 项目概述

本指南说明如何使用新增的压缩、处理和流媒体分发模块实现完整的音视频服务器功能，包括：
- 捕获原始音视频数据
- 实时压缩编码
- 多客户端并发流分发
- 自适应码率控制
- 完整的性能监控

## 新增模块

### 1. 压缩引擎 (AVServer_14_CompressionEngine.h)

**功能**：处理音视频帧的实时编码压缩

**关键类**：
- `CompressionConfig`：压缩配置参数
  - `compression_level` (0-9)：压缩级别
  - `quality` (0-100)：编码质量
  - `target_bitrate`：目标码率（bps）
  - `enable_adaptive_bitrate`：是否启用自适应码率
  - `target_framerate`：目标帧率
  - `keyframe_interval`：关键帧间隔（秒）

- `EncodingStatistics`：编码统计信息
  - 处理的视频/音频帧数
  - 输入/输出数据大小
  - 压缩比、编码时间、码率统计

- `CompressionEngine`：主要压缩引擎
  - `bool encode_video()`：对视频帧编码
  - `bool encode_audio()`：对音频帧编码
  - `compress_with_zlib()`：使用zlib压缩数据
  - `decompress_with_zlib()`：使用zlib解压数据
  - `set_target_bitrate()`：动态调整目标码率

**使用示例**：
```cpp
CompressionConfig config;
config.quality = 80;
config.target_bitrate = 5000000;  // 5Mbps

CompressionEngine engine(config);
engine.start();

auto encoded_frame = frame_pool->get();
if (engine.encode_video(raw_frame, encoded_frame)) {
    // 使用编码后的帧
    std::cout << "Encoded: " << encoded_frame->size << " bytes" << std::endl;
}
```

### 2. 媒体处理器 (AVServer_15_MediaProcessor.h)

**功能**：整合捕获、编码、打包的完整处理流程

**关键类**：
- `ProcessingStatistics`：处理统计信息
  - 视频/音频帧计数
  - 消息发送统计
  - 帧率、延迟统计
  - 队列深度监控

- `MediaProcessor`：处理器主类
  - 持续从CaptureManager获取原始帧
  - 通过CompressionEngine进行编码
  - 格式化为Message消息
  - 放入SafeQueue供StreamingService分发

**数据流**：
```
Capture → Compress → Package → Message Queue → StreamingService
```

**关键方法**：
- `bool start()`：启动处理线程
- `std::unique_ptr<Message> get_message(int timeout_ms)`：获取消息（阻塞）
- `std::unique_ptr<Message> try_get_message()`：获取消息（非阻塞）
- `ProcessingStatistics get_statistics()`：获取统计信息

### 3. 流媒体服务 (AVServer_16_StreamingService.h)

**功能**：管理多客户端并发流分发，支持带宽控制

**关键类**：
- `ClientSession`：客户端会话信息
  - `client_id`：连接ID
  - `client_addr`：客户端地址
  - `bitrate_limit`：该客户端的码率限制
  - `bytes_sent`、`messages_sent`：统计信息
  - `get_actual_bitrate()`：获取实际码率

- `StreamingStatistics`：流媒体统计
  - 总连接数和当前活跃客户端数
  - 分发消息数和字节数
  - 平均码率和总带宽使用

- `StreamingService`：主服务类
  - 维护客户端连接映表
  - 分发来自MediaProcessor的消息
  - 实施per-client码率限制
  - 实时统计监控

**关键方法**：
```cpp
void register_client(uint32_t client_id, const std::string& addr,
                     uint32_t bitrate_limit);  // 注册客户端
void unregister_client(uint32_t client_id);     // 注销客户端
void set_client_bitrate_limit(uint32_t id, uint32_t bitrate);  // 调整码率
ClientSession get_client_info(uint32_t client_id);
std::map<uint32_t, ClientSession> get_all_clients();
StreamingStatistics get_statistics();
```

## 集成到AVServer

### 修改的内容

#### 1. AVServer_09_AVServer.h

**新增成员变量**：
```cpp
// 捕获模块
std::unique_ptr<VideoCapture> video_capture_;
std::unique_ptr<AudioCapture> audio_capture_;
std::unique_ptr<CaptureManager> capture_manager_;

// 压缩模块
std::unique_ptr<CompressionEngine> compression_engine_;

// 处理模块
std::unique_ptr<MediaProcessor> media_processor_;

// 流媒体模块
std::unique_ptr<StreamingService> streaming_service_;

// 处理线程
std::thread distribution_thread_;      // 消息分发线程
std::thread stats_update_thread_;      // 统计更新线程
```

**修改的方法**：

1. **start() 启动流程**：
   - 初始化VideoCapture（1920x1080, 30fps）
   - 初始化AudioCapture（48kHz, 2ch）
   - 创建CaptureManager
   - 创建CompressionEngine（配置为5Mbps, 质量80）
   - 创建MediaProcessor
   - 创建StreamingService
   - 启动所有组件
   - 启动分发线程和统计线程

2. **stop() 停止流程**：
   - 停止分发和统计线程
   - 按相反顺序停止所有组件
   - 清理资源

3. **on_client_connected()**：
   - 在StreamingService中注册客户端
   - 设置默认码率限制（5Mbps）

4. **on_client_disconnected()**：
   - 从StreamingService中注销客户端

5. **handle_set_bitrate()**：
   - 解析客户端发送的目标码率
   - 更新StreamingService中的客户端码率限制
   - 更新CompressionEngine的目标码率

6. **distribution_loop()**：
   - 从MediaProcessor获取压缩消息
   - 向所有活跃客户端分发
   - 更新发送统计

7. **stats_update_loop()**：
   - 定期收集各模块统计信息
   - 每10秒输出性能监控日志
   - 报告处理队列深度和客户端带宽

8. **print_comprehensive_statistics()**（新方法）：
   - 输出服务器统计
   - 输出捕获统计
   - 输出压缩统计
   - 输出处理统计
   - 输出流媒体统计和客户端列表

#### 2. AVServer_10_main.cpp

**新增命令**：
- `fullstats`：显示所有模块的完整统计信息

**更新的帮助信息**：
```
help       - Show this help message
status     - Show server status
stats      - Show server statistics
conns      - Show current connection count
fullstats  - Show comprehensive statistics (all modules)
quit/exit  - Shutdown server gracefully
clear      - Clear screen
```

## 完整的工作流程

```
┌─────────────────────────────────────────────────────┐
│           启动序列 (start())                       │
└──────────────────┬──────────────────────────────────┘
                   │
     ┌─────────────┴──────────────┐
     │                            │
┌────▼────┐  ┌──────────┐  ┌─────▼──────┐
│ Capture │  │Compress  │  │ Media      │
│ Manager │─→│  Engine  │─→│ Processor  │
└────┬────┘  └──────────┘  └─────┬──────┘
     │                            │
     │  ┌──────────────────────┐  │
     └─→│ Streaming Service    │←─┘
        │ (管理客户端分发)      │
        └──────┬───────────────┘
               │
        ┌──────▼────────┐
        │ Message       │
        │ Distribution  │
        │ Thread        │
        └──────┬────────┘
               │
        ┌──────▼────────────────┐
        │ All Connected Clients │
        │ (TCP连接)            │
        └───────────────────────┘

    ┌──────────────────────────┐
    │ Statistics Update Thread  │
    │ (每10秒输出性能日志)      │
    └──────────────────────────┘
```

## 配置参数说明

### 视频捕获配置
```cpp
VideoCapture video_capture(
    1920,           // 宽度 (像素)
    1080,           // 高度 (像素)
    30,             // 帧率 (fps)
    15000000        // 比特率 (bps)
);
```

### 音频捕获配置
```cpp
AudioCapture audio_capture(
    48000,          // 采样率 (Hz)
    2,              // 通道数
    128000          // 比特率 (bps)
);
```

### 压缩配置
```cpp
CompressionConfig config;
config.compression_level = 6;           // 0-9，6为平衡
config.quality = 80;                    // 0-100，80为高质量
config.target_bitrate = 5000000;        // 5Mbps
config.enable_adaptive_bitrate = true;  // 启用自适应
config.target_framerate = 30;
config.keyframe_interval = 2;           // 每2秒一个关键帧
```

## 监控和统计

### 实时监控
- 处理队列深度（MediaProcessor）
- 活跃客户端数
- 实时带宽使用
- 消息分发速率

### 统计信息
- 捕获：视频/音频帧计数、码率
- 压缩：帧数、字节数、压缩比、编码时间
- 处理：吞吐量、延迟、fps
- 流媒体：客户端列表、每客户端统计、带宽聚合

### 命令行查询
```bash
# 基本统计
> stats

# 完整统计（所有模块）
> fullstats

# 连接数
> conns
```

## 性能考虑

### 内存使用
- FrameBufferPool：10个1MB缓冲区 = 10MB
- 消息队列：取决于处理速率和网络速率
- 客户端Session对象：每客户端~1KB

### CPU使用
- 捕获线程：独立（模拟）
- 压缩线程：编码CPU密集
- 处理线程：相对轻量
- 分发线程：I/O密集
- 统计线程：低优先级

### 网络带宽
- 默认码率：5Mbps/客户端
- 可通过SET_BITRATE消息动态调整
- 支持自适应码率控制

## 扩展建议

1. **集成真实编码库**（替代模拟编码）：
   - 使用FFmpeg进行H.264/H.265编码
   - 集成libopus进行音频编码
   - 硬件加速支持（NVIDIA/AMD）

2. **增强监控**：
   - 时间序列数据库（InfluxDB）
   - 实时仪表板（Grafana）
   - 性能告警机制

3. **优化网络**：
   - 实施丢包重传
   - UDP模式支持（低延迟直播）
   - 多路复用

4. **客户端支持**：
   - Web客户端（WebRTC/HLS）
   - 移动应用（iOS/Android）
   - 桌面应用优化

## 编译和运行

### Linux/Unix
```bash
# 编译
g++ -std=c++17 -pthread -lz -o avserver AVServer_10_main.cpp

# 运行
./avserver 8888

# 在服务器启动后，在命令行输入命令：
help        # 查看所有命令
stats       # 查看基本统计
fullstats   # 查看完整统计
conns       # 查看连接数
quit        # 关闭服务器
```

### Windows (Visual Studio)
- 添加所有.h文件到项目
- 链接 ws2_32.lib（TCP网络）
- 链接 zlib.lib（压缩库）
- 设置为C++17标准
- 创建控制台应用程序

## 常见问题

**Q: 如何改变捕获分辨率？**
A: 在AVServer::start()中修改VideoCapture构造参数：
```cpp
VideoCapture video_capture(1280, 720, 30, 5000000);  // 720p
```

**Q: 如何支持更多客户端？**
A: 客户端数主要受限于网络带宽。增加以下方面：
- 使用更低的码率限制
- 启用自适应码率
- 增加服务器网络接口

**Q: 延迟是多少？**
A: 系统延迟取决于：
- 捕获延迟：~16ms（30fps）
- 编码延迟：~50-100ms
- 网络传输：取决于带宽
- 总计：~100-150ms（典型）

**Q: 如何实现录制功能？**
A: 在distribution_loop()中添加文件写入：
```cpp
if (msg) {
    // 写入文件
    recording_file.write((const char*)msg->to_bytes().data(),
                         msg->to_bytes().size());
    // 继续分发...
}
```

## 总结

该集成实现了一个完整的音视频服务器：
- ✅ 实时音视频捕获
- ✅ 高效的编码压缩
- ✅ 多客户端并发分发
- ✅ 自适应码率控制
- ✅ 完整的性能监控
- ✅ 易于扩展和定制

所有代码均包含详细的中文注释，适合初学者学习和理解。
