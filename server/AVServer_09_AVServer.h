/*
 * AVServer.h - 音视频服务器应用程序
 *
 * 功能：
 * - 整合所有组件（TcpServer, ThreadPool, Codec, Buffer等）
 * - 管理应用程序的生命周期
 * - 处理客户端请求和消息分发
 * - 管理编解码器
 * - 统计和监控服务器状态
 *
 * 设计特点：
 * - 模块化设计，各组件独立
 * - 事件驱动的消息处理
 * - 自动资源管理
 * - 详细的日志和统计信息
 *
 * 使用场景：
 * - 音视频实时流媒体服务
 * - 多客户端并发处理
 * - 动态码率自适应
 * - 连接管理和监控
 */

#ifndef AV_SERVER_H
#define AV_SERVER_H

#include <memory>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "AVServer_07_TcpServer.h"
#include "AVServer_08_Connection.h"
#include "AVServer_03_FrameBuffer.h"
#include "AVServer_11_VideoCapture.h"
#include "AVServer_12_AudioCapture.h"
#include "AVServer_13_CaptureManager.h"
#include "AVServer_14_CompressionEngine.h"
#include "AVServer_15_MediaProcessor.h"
#include "AVServer_16_StreamingService.h"

// ============================================================================
// ======================== 服务器状态统计 =====================================
// ============================================================================

/**
 * @struct ServerStatistics
 * @brief 服务器统计信息
 *
 * 记录：
 * - 总连接数和当前连接数
 * - 接收/发送的消息数和字节数
 * - 处理的视频/音频帧数
 * - 性能指标（帧率、码率等）
 */
struct ServerStatistics {
    // 连接统计
    uint64_t total_connections;             // 总接受的连接数
    uint64_t current_connections;           // 当前活跃连接数

    // 消息统计
    uint64_t total_messages_received;       // 接收的总消息数
    uint64_t total_messages_sent;           // 发送的总消息数
    uint64_t total_bytes_received;          // 接收的总字节数
    uint64_t total_bytes_sent;              // 发送的总字节数

    // 帧统计
    uint64_t video_frames_received;         // 接收的视频帧数
    uint64_t audio_frames_received;         // 接收的音频帧数
    uint64_t video_frames_sent;             // 发送的视频帧数
    uint64_t audio_frames_sent;             // 发送的音频帧数

    // 时间信息
    std::chrono::steady_clock::time_point start_time;  // 服务器启动时间

    /**
     * @brief 构造函数 - 初始化所有计数为0
     */
    ServerStatistics()
        : total_connections(0),
          current_connections(0),
          total_messages_received(0),
          total_messages_sent(0),
          total_bytes_received(0),
          total_bytes_sent(0),
          video_frames_received(0),
          audio_frames_received(0),
          video_frames_sent(0),
          audio_frames_sent(0),
          start_time(std::chrono::steady_clock::now()) {
    }

    /**
     * @brief 获取服务器运行时间（秒）
     *
     * @return 运行秒数
     */
    uint64_t get_uptime_seconds() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time).count();
        return elapsed;
    }

    /**
     * @brief 获取统计信息的格式化字符串
     *
     * @return 包含所有统计信息的多行字符串
     */
    std::string to_string() const {
        std::ostringstream oss;
        uint64_t uptime = get_uptime_seconds();

        oss << "=== Server Statistics ===" << std::endl;
        oss << "Uptime: " << uptime << "s" << std::endl;
        oss << "Current Connections: " << current_connections << std::endl;
        oss << "Total Connections: " << total_connections << std::endl;

        oss << "\nMessages:" << std::endl;
        oss << "  Received: " << total_messages_received << std::endl;
        oss << "  Sent: " << total_messages_sent << std::endl;

        oss << "\nBytes:" << std::endl;
        oss << "  Received: " << total_bytes_received << " bytes ("
            << (total_bytes_received / (1024.0 * 1024.0)) << " MB)" << std::endl;
        oss << "  Sent: " << total_bytes_sent << " bytes ("
            << (total_bytes_sent / (1024.0 * 1024.0)) << " MB)" << std::endl;

        oss << "\nFrames:" << std::endl;
        oss << "  Video Received: " << video_frames_received << std::endl;
        oss << "  Audio Received: " << audio_frames_received << std::endl;
        oss << "  Video Sent: " << video_frames_sent << std::endl;
        oss << "  Audio Sent: " << audio_frames_sent << std::endl;

        // 计算吞吐量
        if (uptime > 0) {
            double avg_bitrate = (total_bytes_sent * 8) / static_cast<double>(uptime);
            oss << "\nPerformance:" << std::endl;
            oss << "  Avg Bitrate: " << (avg_bitrate / 1000000.0) << " Mbps" << std::endl;
            oss << "  Avg Video FPS: " << (video_frames_sent / static_cast<double>(uptime))
                << " fps" << std::endl;
        }

        return oss.str();
    }
};

// ============================================================================
// ======================== 音视频服务器应用 ===================================
// ============================================================================

/**
 * @class AVServer
 * @brief 音视频服务器应用程序
 *
 * 核心功能：
 * 1. 启动和管理TCP服务器
 * 2. 处理客户端连接和消息
 * 3. 管理编解码器和帧缓冲池
 * 4. 统计服务器性能指标
 * 5. 提供事件回调接口
 *
 * 使用流程：
 * @code
 *   // 1. 创建服务器
 *   AVServer server;
 *
 *   // 2. 配置服务器参数
 *   auto config = server.get_config();
 *   config.port = 8888;
 *   config.thread_pool_size = 8;
 *
 *   // 3. 启动服务器
 *   if (!server.start()) {
 *       std::cerr << "Failed to start server" << std::endl;
 *       return 1;
 *   }
 *
 *   // 4. 服务器运行
 *   // 处理信号或其他控制逻辑
 *   // ...
 *
 *   // 5. 停止服务器
 *   server.stop();
 * @endcode
 *
 * @note 该类整合了所有组件，作为应用程序的主要入口点
 */
class AVServer {
public:
    /**
     * @brief 构造函数
     *
     * @param config 服务器配置（可选，使用默认配置）
     */
    explicit AVServer(const ServerConfig& config = ServerConfig())
        : tcp_server_(config),
          frame_buffer_pool_(10, 1024 * 1024),  // 10个缓冲区，每个1MB
          running_(false),
          stats_(),
          video_capture_(nullptr),
          audio_capture_(nullptr),
          capture_manager_(nullptr),
          compression_engine_(nullptr),
          media_processor_(nullptr),
          streaming_service_(nullptr),
          distribution_thread_(),
          stats_update_thread_() {

        // 注册TCP服务器的回调
        tcp_server_.set_on_client_connected(
            [this](const std::shared_ptr<Connection>& conn) {
                on_client_connected(conn);
            });

        tcp_server_.set_on_message_received(
            [this](const std::shared_ptr<Connection>& conn, const Message& msg) {
                on_message_received(conn, msg);
            });

        tcp_server_.set_on_client_disconnected(
            [this](const std::shared_ptr<Connection>& conn) {
                on_client_disconnected(conn);
            });
    }

    /**
     * @brief 析构函数
     * 自动停止服务器
     */
    ~AVServer() {
        stop();
    }

    /**
     * @brief 启动服务器
     *
     * @return true 如果服务器启动成功
     *
     * @note 该函数会启动TCP服务器并初始化各个组件
     * @note 启动流程：
     *       1. 初始化音视频捕获模块
     *       2. 初始化压缩引擎
     *       3. 初始化媒体处理器
     *       4. 初始化流媒体服务
     *       5. 启动TCP服务器
     *       6. 启动消息分发线程
     *       7. 启动统计更新线程
     */
    bool start() {
        if (running_.load()) {
            return true;
        }

        // ===== 1. 初始化音视频捕获 =====
        std::cout << "[AVServer] Initializing capture modules..." << std::endl;

        video_capture_ = std::make_unique<VideoCapture>(
            1920, 1080,         // 分辨率
            30,                 // 帧率
            15000000            // 比特率 (15Mbps)
        );

        audio_capture_ = std::make_unique<AudioCapture>(
            48000,              // 采样率
            2,                  // 通道数
            128000              // 比特率 (128kbps)
        );

        capture_manager_ = std::make_unique<CaptureManager>(
            video_capture_.get(),
            audio_capture_.get()
        );

        // 启动捕获
        if (!capture_manager_->start()) {
            std::cerr << "[AVServer] Failed to start capture manager" << std::endl;
            return false;
        }

        // ===== 2. 初始化压缩引擎 =====
        std::cout << "[AVServer] Initializing compression engine..." << std::endl;

        CompressionConfig compression_config;
        compression_config.compression_level = 6;
        compression_config.quality = 80;
        compression_config.target_bitrate = 5000000;      // 5Mbps
        compression_config.enable_adaptive_bitrate = true;
        compression_config.target_framerate = 30;
        compression_config.keyframe_interval = 2;

        compression_engine_ = std::make_unique<CompressionEngine>(compression_config);

        if (!compression_engine_->start()) {
            std::cerr << "[AVServer] Failed to start compression engine" << std::endl;
            return false;
        }

        // ===== 3. 初始化媒体处理器 =====
        std::cout << "[AVServer] Initializing media processor..." << std::endl;

        media_processor_ = std::make_unique<MediaProcessor>(
            capture_manager_.get(),
            compression_engine_.get()
        );

        if (!media_processor_->start()) {
            std::cerr << "[AVServer] Failed to start media processor" << std::endl;
            return false;
        }

        // ===== 4. 初始化流媒体服务 =====
        std::cout << "[AVServer] Initializing streaming service..." << std::endl;

        streaming_service_ = std::make_unique<StreamingService>(
            media_processor_.get()
        );

        if (!streaming_service_->start()) {
            std::cerr << "[AVServer] Failed to start streaming service" << std::endl;
            return false;
        }

        // ===== 5. 启动TCP服务器 =====
        std::cout << "[AVServer] Starting TCP server..." << std::endl;
        if (!tcp_server_.start()) {
            std::cerr << "[AVServer] Failed to start TCP server" << std::endl;
            return false;
        }

        // ===== 6. 启动消息分发线程 =====
        std::cout << "[AVServer] Starting message distribution thread..." << std::endl;
        distribution_thread_ = std::thread(&AVServer::distribution_loop, this);

        // ===== 7. 启动统计更新线程 =====
        std::cout << "[AVServer] Starting statistics update thread..." << std::endl;
        stats_update_thread_ = std::thread(&AVServer::stats_update_loop, this);

        running_ = true;
        std::cout << "[AVServer] All components started successfully" << std::endl;
        return true;
    }

    /**
     * @brief 停止服务器
     *
     * 停止步骤：
     * 1. 停止消息分发线程
     * 2. 停止统计更新线程
     * 3. 停止流媒体服务
     * 4. 停止媒体处理器
     * 5. 停止压缩引擎
     * 6. 停止捕获管理器
     * 7. 停止TCP服务器
     * 8. 清空帧缓冲池
     * 9. 输出最终统计信息
     *
     * @note 该函数会等待所有操作完成再返回
     * @note 所有组件会按相反顺序进行关闭
     */
    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {
            // 已经停止
            return;
        }

        std::cout << "[AVServer] Initiating shutdown sequence..." << std::endl;

        // ===== 1. 停止分发线程 =====
        std::cout << "[AVServer] Stopping distribution thread..." << std::endl;
        if (distribution_thread_.joinable()) {
            distribution_thread_.join();
        }

        // ===== 2. 停止统计线程 =====
        std::cout << "[AVServer] Stopping statistics thread..." << std::endl;
        if (stats_update_thread_.joinable()) {
            stats_update_thread_.join();
        }

        // ===== 3. 停止流媒体服务 =====
        std::cout << "[AVServer] Stopping streaming service..." << std::endl;
        if (streaming_service_) {
            streaming_service_->stop();
            streaming_service_.reset();
        }

        // ===== 4. 停止媒体处理器 =====
        std::cout << "[AVServer] Stopping media processor..." << std::endl;
        if (media_processor_) {
            media_processor_->stop();
            media_processor_.reset();
        }

        // ===== 5. 停止压缩引擎 =====
        std::cout << "[AVServer] Stopping compression engine..." << std::endl;
        if (compression_engine_) {
            compression_engine_->stop();
            compression_engine_.reset();
        }

        // ===== 6. 停止捕获管理器 =====
        std::cout << "[AVServer] Stopping capture manager..." << std::endl;
        if (capture_manager_) {
            capture_manager_->stop();
            capture_manager_.reset();
        }

        // ===== 7. 停止TCP服务器 =====
        std::cout << "[AVServer] Stopping TCP server..." << std::endl;
        tcp_server_.stop();

        // ===== 8. 清空帧缓冲池 =====
        frame_buffer_pool_.clear();

        // ===== 9. 输出最终统计信息 =====
        std::cout << "\n[AVServer] Final Statistics:" << std::endl;
        print_comprehensive_statistics();

        std::cout << "[AVServer] Server shutdown complete" << std::endl;
    }

    /**
     * @brief 检查服务器是否正在运行
     *
     * @return true 如果服务器正在运行
     */
    bool is_running() const {
        return running_.load();
    }

    /**
     * @brief 获取统计信息
     *
     * @return 服务器统计信息
     */
    ServerStatistics get_statistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        ServerStatistics stats = stats_;
        stats.current_connections = tcp_server_.get_connection_count();
        return stats;
    }

    /**
     * @brief 输出统计信息
     *
     * @note 输出到标准输出
     */
    void print_statistics() const {
        std::cout << get_statistics().to_string() << std::endl;
    }

    /**
     * @brief 输出完整的统计信息
     *
     * 包括：
     * - 服务器统计信息
     * - 捕获管理器统计信息
     * - 压缩引擎统计信息
     * - 媒体处理器统计信息
     * - 流媒体服务统计信息
     *
     * @note 输出到标准输出
     */
    void print_comprehensive_statistics() const {
        std::cout << "\n[AVServer] ===== 服务器统计 =====" << std::endl;
        print_statistics();

        if (capture_manager_) {
            std::cout << "\n[AVServer] ===== 捕获统计 =====" << std::endl;
            auto capture_stats = capture_manager_->get_statistics();
            std::cout << "Video Frames: " << capture_stats.video_frames_captured << std::endl;
            std::cout << "Audio Frames: " << capture_stats.audio_frames_captured << std::endl;
        }

        if (compression_engine_) {
            std::cout << "\n[AVServer] ===== 压缩统计 =====" << std::endl;
            compression_engine_->print_statistics();
        }

        if (media_processor_) {
            std::cout << "\n[AVServer] ===== 处理统计 =====" << std::endl;
            media_processor_->print_statistics();
        }

        if (streaming_service_) {
            std::cout << "\n[AVServer] ===== 流媒体统计 =====" << std::endl;
            streaming_service_->print_statistics();
            streaming_service_->print_clients_info();
        }
    }

    /**
     * @brief 获取TCP服务器引用
     *
     * @return 对TCP服务器的常量引用
     *
     * @note 用于访问TCP服务器的接口
     */
    const TcpServer& get_tcp_server() const {
        return tcp_server_;
    }

    /**
     * @brief 获取TCP服务器的配置
     *
     * @return 服务器配置的常量引用
     */
    const ServerConfig& get_config() const {
        return tcp_server_.get_config();
    }

    /**
     * @brief 获取帧缓冲池
     *
     * @return 帧缓冲池的引用
     *
     * @note 用于从池中获取或归还帧缓冲
     */
    FrameBufferPool& get_frame_buffer_pool() {
        return frame_buffer_pool_;
    }

    /**
     * @brief 向所有客户端广播消息
     *
     * @param[in] message 要发送的消息
     *
     * @note 向所有活跃连接发送相同的消息
     */
    void broadcast(const Message& message) {
        tcp_server_.broadcast(message);

        // 更新发送统计
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_messages_sent++;
        auto bytes = message.to_bytes();
        stats_.total_bytes_sent += bytes.size();
    }

    /**
     * @brief 向指定客户端发送消息
     *
     * @param connection_id 客户端连接ID
     * @param[in] message 要发送的消息
     * @return true 如果发送成功
     */
    bool send_to_client(uint32_t connection_id, const Message& message) {
        auto conn = tcp_server_.get_connection(connection_id);
        if (!conn) {
            return false;
        }

        bool success = conn->send(message);
        if (success) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_messages_sent++;
            auto bytes = message.to_bytes();
            stats_.total_bytes_sent += bytes.size();
        }

        return success;
    }

private:
    /**
     * @brief 客户端连接事件处理
     *
     * @param connection 新连接对象
     *
     * @note 在客户端连接时被调用
     * @note 可以进行连接初始化和在流媒体服务中注册客户端
     * @note 流程：
     *       1. 更新连接统计
     *       2. 在流媒体服务中注册客户端
     *       3. 发送欢迎消息
     */
    void on_client_connected(const std::shared_ptr<Connection>& connection) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_connections++;

        std::cout << "[AVServer] Client connected: " << connection->get_addr()
                  << " (ID: " << connection->get_id() << ")" << std::endl;

        // 在流媒体服务中注册客户端（用于接收压缩的流媒体数据）
        if (streaming_service_) {
            streaming_service_->register_client(
                connection->get_id(),
                connection->get_addr(),
                5000000  // 默认码率限制：5Mbps
            );
            std::cout << "[AVServer] Client registered with streaming service" << std::endl;
        }

        // 发送欢迎消息
        Message welcome(MessageType::ACK, 0, ProtocolHelper::get_timestamp_ms());
        connection->send(welcome);
    }

    /**
     * @brief 接收消息事件处理
     *
     * @param connection 接收消息的连接
     * @param message 接收到的消息
     *
     * @note 在接收完整消息时被调用
     * @note 在线程池线程中执行
     */
    void on_message_received(const std::shared_ptr<Connection>& connection,
                            const Message& message) {
        // 更新接收统计
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_messages_received++;

            auto bytes = message.to_bytes();
            stats_.total_bytes_received += bytes.size();
        }

        // 根据消息类型处理
        MessageType msg_type = message.get_type();

        switch (msg_type) {
            case MessageType::VIDEO_FRAME:
                handle_video_frame(connection, message);
                break;

            case MessageType::AUDIO_FRAME:
                handle_audio_frame(connection, message);
                break;

            case MessageType::START_STREAM:
                handle_start_stream(connection, message);
                break;

            case MessageType::STOP_STREAM:
                handle_stop_stream(connection, message);
                break;

            case MessageType::SET_BITRATE:
                handle_set_bitrate(connection, message);
                break;

            case MessageType::HEARTBEAT:
                handle_heartbeat(connection, message);
                break;

            default:
                std::cout << "Unknown message type: "
                         << ProtocolHelper::message_type_to_string(msg_type) << std::endl;
                break;
        }
    }

    /**
     * @brief 客户端断开连接事件处理
     *
     * @param connection 断开连接的连接对象
     *
     * @note 在客户端断开连接时被调用
     * @note 流程：
     *       1. 从流媒体服务中注销客户端
     *       2. 输出连接关闭信息
     */
    void on_client_disconnected(const std::shared_ptr<Connection>& connection) {
        std::cout << "[AVServer] Client disconnected: " << connection->get_addr()
                  << " (ID: " << connection->get_id() << ")" << std::endl;

        // 从流媒体服务中注销客户端
        if (streaming_service_) {
            streaming_service_->unregister_client(connection->get_id());
            std::cout << "[AVServer] Client unregistered from streaming service" << std::endl;
        }
    }

    /**
     * @brief 处理视频帧
     *
     * @param connection 发送帧的客户端连接
     * @param message 包含视频帧的消息
     */
    void handle_video_frame(const std::shared_ptr<Connection>& connection,
                           const Message& message) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.video_frames_received++;

        // TODO: 实现视频帧处理逻辑
        // 可能的操作：
        // 1. 获取帧缓冲
        // 2. 复制消息数据到帧缓冲
        // 3. 进行编解码处理
        // 4. 转发给其他客户端或保存
    }

    /**
     * @brief 处理音频帧
     *
     * @param connection 发送帧的客户端连接
     * @param message 包含音频帧的消息
     */
    void handle_audio_frame(const std::shared_ptr<Connection>& connection,
                           const Message& message) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.audio_frames_received++;

        // TODO: 实现音频帧处理逻辑
    }

    /**
     * @brief 处理开始流传输命令
     *
     * @param connection 发送命令的客户端连接
     * @param message 命令消息
     */
    void handle_start_stream(const std::shared_ptr<Connection>& connection,
                            const Message& message) {
        std::cout << "Start stream request from: " << connection->get_addr() << std::endl;

        // 发送ACK
        Message ack(MessageType::ACK, 0, ProtocolHelper::get_timestamp_ms());
        connection->send(ack);
    }

    /**
     * @brief 处理停止流传输命令
     *
     * @param connection 发送命令的客户端连接
     * @param message 命令消息
     */
    void handle_stop_stream(const std::shared_ptr<Connection>& connection,
                           const Message& message) {
        std::cout << "Stop stream request from: " << connection->get_addr() << std::endl;

        // 发送ACK
        Message ack(MessageType::ACK, 0, ProtocolHelper::get_timestamp_ms());
        connection->send(ack);
    }

    /**
     * @brief 处理设置码率命令
     *
     * @param connection 发送命令的客户端连接
     * @param message 命令消息
     *
     * @note 用于客户端动态调整其码率限制
     * @note 消息格式：[bitrate:4 bytes (uint32_t)]
     */
    void handle_set_bitrate(const std::shared_ptr<Connection>& connection,
                           const Message& message) {
        // 从消息体中解析码率值
        // 预期格式：4字节的uint32_t码率值
        auto payload = message.get_payload();

        if (payload.size() >= 4) {
            // 解析码率值（小端序）
            uint32_t bitrate = 0;
            bitrate |= (static_cast<uint32_t>(payload[0]) << 0);
            bitrate |= (static_cast<uint32_t>(payload[1]) << 8);
            bitrate |= (static_cast<uint32_t>(payload[2]) << 16);
            bitrate |= (static_cast<uint32_t>(payload[3]) << 24);

            std::cout << "[AVServer] Set bitrate request from: " << connection->get_addr()
                      << " Bitrate: " << bitrate << " bps (" << (bitrate / 1000000.0)
                      << " Mbps)" << std::endl;

            // 设置客户端的码率限制
            if (streaming_service_) {
                streaming_service_->set_client_bitrate_limit(connection->get_id(), bitrate);
            }

            // 同时更新压缩引擎的目标码率
            if (compression_engine_) {
                compression_engine_->set_target_bitrate(bitrate);
            }
        } else {
            std::cout << "[AVServer] Invalid bitrate message format" << std::endl;
        }

        // 发送ACK
        Message ack(MessageType::ACK, 0, ProtocolHelper::get_timestamp_ms());
        connection->send(ack);
    }

    /**
     * @brief 处理心跳包
     *
     * @param connection 发送心跳的客户端连接
     * @param message 心跳消息
     */
    void handle_heartbeat(const std::shared_ptr<Connection>& connection,
                         const Message& message) {
        // 发送心跳确认
        connection->send_heartbeat_ack();
    }

    /**
     * @brief 消息分发线程主循环
     *
     * 工作流程：
     * 1. 定期从MediaProcessor获取压缩的消息
     * 2. 通过StreamingService分发给所有连接的客户端
     * 3. 更新分发统计信息
     * 4. 监控队列状态
     *
     * @note 在独立线程中运行
     */
    void distribution_loop() {
        while (running_.load()) {
            // 从媒体处理器获取处理后的消息
            if (media_processor_ && streaming_service_) {
                auto msg = media_processor_->try_get_message();

                if (msg) {
                    // 向所有连接的客户端发送
                    // 获取所有客户端
                    auto clients = streaming_service_->get_all_clients();

                    for (const auto& [client_id, session] : clients) {
                        if (session.is_active) {
                            // 向客户端发送消息
                            auto conn = tcp_server_.get_connection(client_id);
                            if (conn) {
                                conn->send(*msg);

                                // 更新统计
                                {
                                    std::lock_guard<std::mutex> lock(stats_mutex_);
                                    if (msg->get_type() == MessageType::VIDEO_FRAME) {
                                        stats_.video_frames_sent++;
                                    } else if (msg->get_type() == MessageType::AUDIO_FRAME) {
                                        stats_.audio_frames_sent++;
                                    }
                                    stats_.total_messages_sent++;
                                    stats_.total_bytes_sent += msg->to_bytes().size();
                                }
                            }
                        }
                    }
                } else {
                    // 队列为空，短暂睡眠以避免忙轮询
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    /**
     * @brief 统计信息更新线程主循环
     *
     * 工作流程：
     * 1. 定期收集各组件的统计信息
     * 2. 更新流媒体服务的带宽统计
     * 3. 监控处理队列深度
     * 4. 定期输出性能日志
     *
     * @note 在独立线程中运行
     * @note 每10秒输出一次性能日志
     */
    void stats_update_loop() {
        int log_interval = 0;
        const int LOG_INTERVAL_THRESHOLD = 10;  // 每10次迭代（约10秒）输出一次

        while (running_.load()) {
            // 更新统计信息
            if (media_processor_) {
                auto proc_stats = media_processor_->get_statistics();
                // 统计信息已在处理器中维护
            }

            if (compression_engine_) {
                auto compress_stats = compression_engine_->get_statistics();
                // 统计信息已在压缩引擎中维护
            }

            if (streaming_service_) {
                auto stream_stats = streaming_service_->get_statistics();
                // 统计信息已在流媒体服务中维护
            }

            // 定期输出性能日志
            log_interval++;
            if (log_interval >= LOG_INTERVAL_THRESHOLD) {
                log_interval = 0;

                std::cout << "\n[AVServer] === Performance Monitor ===" << std::endl;

                if (media_processor_) {
                    std::cout << "[AVServer] Media Processor: Queue size = "
                              << media_processor_->get_queue_size() << std::endl;
                }

                if (streaming_service_) {
                    auto stats = streaming_service_->get_statistics();
                    std::cout << "[AVServer] Streaming Service: Active clients = "
                              << stats.current_active_clients
                              << ", Messages: " << stats.total_messages_distributed
                              << ", Bandwidth: " << (stats.total_bandwidth_usage / 1000000.0)
                              << " Mbps" << std::endl;
                }

                std::cout << "[AVServer] ==========================\n" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    // ===== TCP和连接管理 =====
    TcpServer tcp_server_;                          // TCP服务器
    FrameBufferPool frame_buffer_pool_;             // 帧缓冲池

    // ===== 音视频捕获模块 =====
    std::unique_ptr<VideoCapture> video_capture_;           // 视频捕获模块
    std::unique_ptr<AudioCapture> audio_capture_;           // 音频捕获模块
    std::unique_ptr<CaptureManager> capture_manager_;       // 统一捕获管理

    // ===== 编码压缩模块 =====
    std::unique_ptr<CompressionEngine> compression_engine_; // 压缩和编码引擎

    // ===== 媒体处理流程 =====
    std::unique_ptr<MediaProcessor> media_processor_;       // 媒体处理器

    // ===== 流媒体分发 =====
    std::unique_ptr<StreamingService> streaming_service_;   // 流媒体服务

    // ===== 运行状态 =====
    std::atomic<bool> running_;                             // 运行状态标志
    std::thread distribution_thread_;                       // 消息分发线程
    std::thread stats_update_thread_;                       // 统计更新线程

    // ===== 统计信息 =====
    mutable std::mutex stats_mutex_;                        // 保护统计信息的互斥锁
    ServerStatistics stats_;                                // 服务器统计信息
};

#endif // AV_SERVER_H
