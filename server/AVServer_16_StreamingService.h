/*
 * StreamingService.h - 流媒体服务
 *
 * 功能：
 * - 管理多个客户端的流媒体连接
 * - 自适应码率控制
 * - 带宽管理和流量控制
 * - 连接状态监控
 * - 性能统计和监控
 *
 * 设计特点：
 * - 支持多客户端并发直播
 * - 自动的带宽自适应
 * - 精细的流量控制
 * - 完整的监控和统计
 *
 * 使用场景：
 * - 实时视频直播服务
 * - 多客户端并发传输
 * - 自适应码率直播
 * - 带宽优化和管理
 */

#ifndef STREAMING_SERVICE_H
#define STREAMING_SERVICE_H

#include <memory>
#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>
#include <iostream>

#include "AVServer_15_MediaProcessor.h"
#include "AVServer_07_TcpServer.h"

// ============================================================================
// ======================== 客户端流媒体会话 ===================================
// ============================================================================

/**
 * @struct ClientSession
 * @brief 客户端流媒体会话信息
 */
struct ClientSession {
    uint32_t client_id;                 // 客户端连接ID
    std::string client_addr;            // 客户端地址
    uint32_t bitrate_limit;             // 该客户端的码率限制（bps）
    uint64_t bytes_sent;                // 已发送的字节数
    uint64_t messages_sent;             // 已发送的消息数
    std::chrono::steady_clock::time_point start_time;  // 会话开始时间
    bool is_active;                     // 是否仍在活跃

    /**
     * @brief 构造函数
     */
    ClientSession(uint32_t id = 0, const std::string& addr = "")
        : client_id(id),
          client_addr(addr),
          bitrate_limit(5000000),  // 默认5Mbps
          bytes_sent(0),
          messages_sent(0),
          start_time(std::chrono::steady_clock::now()),
          is_active(true) {
    }

    /**
     * @brief 获取会话运行时间（秒）
     *
     * @return 运行秒数
     */
    uint64_t get_duration_seconds() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time).count();
        return elapsed;
    }

    /**
     * @brief 获取实际码率
     *
     * @return 实际码率（bps）
     */
    uint32_t get_actual_bitrate() const {
        auto duration = get_duration_seconds();
        if (duration == 0) return 0;
        return (bytes_sent * 8) / duration;
    }
};

// ============================================================================
// ======================== 流媒体统计 ========================================
// ============================================================================

/**
 * @struct StreamingStatistics
 * @brief 流媒体服务的统计信息
 */
struct StreamingStatistics {
    uint64_t total_clients_connected;    // 总连接客户端数
    uint32_t current_active_clients;     // 当前活跃客户端数
    uint64_t total_messages_distributed; // 总转发的消息数
    uint64_t total_bytes_distributed;    // 总转发的字节数
    double average_client_bitrate;       // 平均客户端码率
    double total_bandwidth_usage;        // 总带宽使用

    /**
     * @brief 构造函数
     */
    StreamingStatistics()
        : total_clients_connected(0),
          current_active_clients(0),
          total_messages_distributed(0),
          total_bytes_distributed(0),
          average_client_bitrate(0.0),
          total_bandwidth_usage(0.0) {
    }

    /**
     * @brief 获取统计信息字符串
     *
     * @return 格式化的统计信息
     */
    std::string to_string() const {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer),
            "Streaming Stats [Clients: %u/%llu, Messages: %llu, "
            "Data: %.2fMB, Avg Bitrate: %.2fMbps, Total Bandwidth: %.2fMbps]",
            current_active_clients, total_clients_connected,
            total_messages_distributed,
            total_bytes_distributed / (1024.0 * 1024.0),
            average_client_bitrate / 1000000.0,
            total_bandwidth_usage / 1000000.0);
        return std::string(buffer);
    }
};

// ============================================================================
// ======================== 流媒体服务 ========================================
// ============================================================================

/**
 * @class StreamingService
 * @brief 流媒体服务管理器
 *
 * 工作流程：
 * 1. 维护客户端连接列表
 * 2. 从MediaProcessor获取编码后的消息
 * 3. 根据客户端码率限制转发消息
 * 4. 监控客户端带宽和性能
 * 5. 自适应调整码率
 *
 * 使用示例：
 * @code
 *   // 创建服务组件
 *   CaptureManager capture;
 *   CompressionEngine engine;
 *   MediaProcessor processor(&capture, &engine);
 *   StreamingService service(&processor);
 *
 *   // 启动
 *   capture.start();
 *   engine.start();
 *   processor.start();
 *   service.start();
 *
 *   // 注册客户端
 *   service.register_client(conn_id, client_addr);
 *
 *   // 获取消息并发送
 *   while (true) {
 *       auto msg = service.get_message_for_client(conn_id);
 *       if (msg) {
 *           connection->send(*msg);
 *       }
 *   }
 *
 *   service.stop();
 * @endcode
 */
class StreamingService {
public:
    /**
     * @brief 构造函数
     *
     * @param processor 媒体处理器指针
     */
    explicit StreamingService(MediaProcessor* processor)
        : processor_(processor),
          running_(false),
          distribution_thread_(),
          clients_(),
          clients_mutex_(),
          stats_(),
          stats_mutex_() {
        std::cout << "[StreamingService] Initialized" << std::endl;
    }

    /**
     * @brief 析构函数
     */
    ~StreamingService() {
        stop();
    }

    /**
     * @brief 启动流媒体服务
     *
     * @return true 如果启动成功
     */
    bool start() {
        if (running_.load()) {
            return true;
        }

        if (!processor_) {
            std::cerr << "[StreamingService] Missing media processor" << std::endl;
            return false;
        }

        running_ = true;
        distribution_thread_ = std::thread(&StreamingService::distribution_loop, this);

        std::cout << "[StreamingService] Started" << std::endl;
        return true;
    }

    /**
     * @brief 停止流媒体服务
     */
    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {
            return;
        }

        // 等待分发线程完成
        if (distribution_thread_.joinable()) {
            distribution_thread_.join();
        }

        // 清理客户端
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.clear();
        }

        std::cout << "[StreamingService] Stopped" << std::endl;
    }

    /**
     * @brief 注册新的客户端
     *
     * @param client_id 客户端连接ID
     * @param client_addr 客户端地址
     * @param bitrate_limit 该客户端的码率限制（bps）
     *
     * @note 当客户端连接时调用
     */
    void register_client(uint32_t client_id, const std::string& client_addr,
                        uint32_t bitrate_limit = 5000000) {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        clients_[client_id] = ClientSession(client_id, client_addr);
        clients_[client_id].bitrate_limit = bitrate_limit;

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.total_clients_connected++;
            stats_.current_active_clients++;
        }

        std::cout << "[StreamingService] Client " << client_id << " (" << client_addr
                  << ") registered" << std::endl;
    }

    /**
     * @brief 注销客户端
     *
     * @param client_id 客户端连接ID
     *
     * @note 当客户端断开连接时调用
     */
    void unregister_client(uint32_t client_id) {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.current_active_clients--;
            }

            std::cout << "[StreamingService] Client " << client_id << " unregistered" << std::endl;
            clients_.erase(it);
        }
    }

    /**
     * @brief 获取客户端的消息队列大小
     *
     * @param client_id 客户端ID
     * @return 该客户端的待发送消息数
     *
     * @note 用于监控客户端的缓冲状态
     */
    size_t get_client_queue_size(uint32_t client_id) const {
        // 这是一个简化实现，实际应该维护每个客户端的消息队列
        return processor_->get_pending_messages();
    }

    /**
     * @brief 设置客户端的码率限制
     *
     * @param client_id 客户端ID
     * @param bitrate 新的码率限制（bps）
     */
    void set_client_bitrate_limit(uint32_t client_id, uint32_t bitrate) {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            it->second.bitrate_limit = bitrate;
            std::cout << "[StreamingService] Client " << client_id
                     << " bitrate limit set to " << bitrate << "bps" << std::endl;
        }
    }

    /**
     * @brief 获取客户端信息
     *
     * @param client_id 客户端ID
     * @return 客户端会话信息
     */
    ClientSession get_client_info(uint32_t client_id) const {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            return it->second;
        }

        return ClientSession();
    }

    /**
     * @brief 获取所有客户端信息
     *
     * @return 客户端会话信息的映射
     */
    std::map<uint32_t, ClientSession> get_all_clients() const {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return clients_;
    }

    /**
     * @brief 获取流媒体统计信息
     *
     * @return 流媒体统计结构体
     */
    StreamingStatistics get_statistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    /**
     * @brief 输出统计信息
     */
    void print_statistics() const {
        std::cout << get_statistics().to_string() << std::endl;
    }

    /**
     * @brief 输出所有客户端的连接信息
     */
    void print_clients_info() const {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        std::cout << "\n=== Connected Clients ===" << std::endl;
        std::cout << "Total: " << clients_.size() << std::endl;

        for (const auto& [id, session] : clients_) {
            std::cout << "  Client #" << id << " " << session.client_addr
                     << " | Bitrate: " << (session.get_actual_bitrate() / 1000000.0)
                     << " Mbps | Duration: " << session.get_duration_seconds() << "s"
                     << " | Sent: " << (session.bytes_sent / (1024.0 * 1024.0)) << " MB"
                     << std::endl;
        }

        std::cout << "" << std::endl;
    }

    /**
     * @brief 检查服务是否正在运行
     *
     * @return true 如果服务正在运行
     */
    bool is_running() const {
        return running_.load();
    }

private:
    /**
     * @brief 消息分发线程主循环
     *
     * 工作流程：
     * 1. 从MediaProcessor获取消息
     * 2. 遍历所有客户端
     * 3. 检查客户端是否需要接收消息
     * 4. 为每个客户端分发消息
     * 5. 更新统计信息
     */
    void distribution_loop() {
        while (running_.load()) {
            // 从处理器获取消息
            auto msg = processor_->try_get_message();

            if (msg) {
                // 向所有客户端分发
                distribute_message(*msg);
            } else {
                // 没有消息，短暂睡眠
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    /**
     * @brief 向所有客户端分发消息
     *
     * @param msg 要分发的消息
     *
     * @note 这是一个简化实现
     * @note 实际应该维护每个客户端的消息队列
     */
    void distribute_message(const Message& msg) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);

        for (auto& [id, session] : clients_) {
            if (session.is_active) {
                // 检查该客户端的码率限制
                // 这里简化处理，实际应该实现更复杂的流量控制

                // 记录统计
                session.bytes_sent += msg.total_size();
                session.messages_sent++;

                stats_.total_messages_distributed++;
                stats_.total_bytes_distributed += msg.total_size();
            }
        }

        // 计算平均码率
        if (!clients_.empty()) {
            uint32_t total_bitrate = 0;
            for (const auto& [id, session] : clients_) {
                total_bitrate += session.get_actual_bitrate();
            }
            stats_.average_client_bitrate = total_bitrate / clients_.size();
            stats_.total_bandwidth_usage = total_bitrate * clients_.size();
        }
    }

private:
    MediaProcessor* processor_;                          // 媒体处理器指针

    std::atomic<bool> running_;                          // 运行状态
    std::thread distribution_thread_;                    // 分发线程

    mutable std::mutex clients_mutex_;                   // 保护客户端映射的互斥锁
    std::map<uint32_t, ClientSession> clients_;          // 客户端会话映射（ID -> Session）

    mutable std::mutex stats_mutex_;                     // 保护统计信息的互斥锁
    StreamingStatistics stats_;                          // 流媒体统计信息
};

#endif // STREAMING_SERVICE_H
