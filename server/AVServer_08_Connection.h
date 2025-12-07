/*
 * Connection.h - 客户端连接管理
 *
 * 功能：
 * - 管理单个TCP客户端连接的生命周期
 * - 接收和发送消息
 * - 消息协议的编解码
 * - 连接状态跟踪
 * - 自动心跳机制
 *
 * 设计特点：
 * - 非阻塞消息接收（使用循环缓冲区）
 * - 消息队列（发送）
 * - 自动心跳和超时检测
 * - 线程安全的状态管理
 *
 * 使用场景：
 * - 处理单个客户端的所有网络通信
 * - 消息的序列化和反序列化
 * - 网络错误和异常处理
 */

#ifndef CONNECTION_H
#define CONNECTION_H

#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>
#include <iostream>
#include <queue>
#include <mutex>

#ifdef _WIN32
    #include <winsock2.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define closesocket close
    typedef int SOCKET;
#endif

#include "AVServer_02_CircularBuffer.h"
#include "AVServer_06_MessageProtocol.h"

// ============================================================================
// ======================== 连接类 ===========================================
// ============================================================================

/**
 * @class Connection
 * @brief 代表一个TCP客户端连接
 *
 * 工作流程：
 * 1. 由TcpServer::accept_loop创建
 * 2. 存储套接字、地址、配置等信息
 * 3. 循环接收数据到循环缓冲区
 * 4. 在缓冲区中查找完整的消息（消息头+消息体）
 * 5. 解析消息并调用回调
 * 6. 处理心跳机制和超时检测
 * 7. 提供发送消息的接口
 * 8. 连接断开时进行清理
 *
 * 线程安全性：
 * - 接收操作：在单个工作线程中执行（不需要额外同步）
 * - 发送操作：可从多个线程调用，使用发送队列
 * - 状态检查：使用原子操作
 *
 * 使用示例：
 * @code
 *   // Connection由TcpServer创建，通常不手动创建
 *   // 但可以访问其接口：
 *
 *   uint32_t id = connection->get_id();
 *   std::string addr = connection->get_addr();
 *
 *   // 发送消息
 *   Message msg(MessageType::VIDEO_FRAME, frame_size);
 *   msg.set_payload(frame_data, frame_size);
 *   connection->send(msg);
 *
 *   // 检查连接状态
 *   if (connection->is_connected()) {
 *       std::cout << "Still connected" << std::endl;
 *   }
 *
 *   // 主动关闭连接
 *   connection->close();
 * @endcode
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    /**
     * @brief 构造函数
     *
     * @param id 连接的唯一ID（由TcpServer分配）
     * @param socket 已连接的TCP套接字
     * @param client_addr 客户端地址信息
     * @param config 服务器配置（用于缓冲区大小、超时等）
     *
     * @note 构造函数不启动接收循环，需要外部启动处理线程
     * @note socket必须是有效的已连接套接字
     */
    Connection(uint32_t id,
              SOCKET socket,
              const struct sockaddr_in& client_addr,
              const struct ServerConfig& config)
        : id_(id),
          socket_(socket),
          client_addr_(client_addr),
          config_(config),
          connected_(true),
          recv_buffer_(config.recv_buffer_size),
          last_activity_time_(std::chrono::steady_clock::now()) {

        // 构建客户端地址字符串
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr_.sin_addr, addr_str, INET_ADDRSTRLEN);
        client_addr_str_ = std::string(addr_str) + ":" +
                          std::to_string(ntohs(client_addr_.sin_port));

        std::cout << "New connection #" << id_ << " from " << client_addr_str_ << std::endl;
    }

    /**
     * @brief 析构函数
     * 自动关闭连接
     */
    ~Connection() {
        close();
    }

    /**
     * @brief 禁用拷贝操作
     * Connection对象不应该被拷贝
     */
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // ===== 连接信息 =====

    /**
     * @brief 获取连接的唯一ID
     *
     * @return 连接ID
     */
    uint32_t get_id() const {
        return id_;
    }

    /**
     * @brief 获取客户端地址（字符串格式）
     *
     * @return "IP:Port"格式的字符串
     *
     * @example "192.168.1.100:54321"
     */
    const std::string& get_addr() const {
        return client_addr_str_;
    }

    /**
     * @brief 获取连接的套接字
     *
     * @return 套接字文件描述符
     *
     * @note 一般不需要直接访问，除非需要特殊处理
     */
    SOCKET get_socket() const {
        return socket_;
    }

    // ===== 连接状态 =====

    /**
     * @brief 检查连接是否仍然活跃
     *
     * @return true 如果连接未被关闭
     */
    bool is_connected() const {
        return connected_.load();
    }

    /**
     * @brief 主动关闭连接
     *
     * 步骤：
     * 1. 设置connected_标志为false
     * 2. 关闭套接字
     * 3. 清空缓冲区
     *
     * @note 可以多次调用，后续调用无效果
     * @note 关闭后无法再收发消息
     */
    void close() {
        bool expected = true;
        if (!connected_.compare_exchange_strong(expected, false)) {
            // 已经关闭
            return;
        }

        // 关闭套接字
        if (socket_ != INVALID_SOCKET) {
            ::closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }

        // 清空缓冲区
        recv_buffer_.clear();

        std::cout << "Connection #" << id_ << " from " << client_addr_str_
                  << " closed" << std::endl;
    }

    // ===== 消息接收 =====

    /**
     * @brief 接收数据并处理消息
     *
     * 工作流程：
     * 1. 从套接字读取数据到循环缓冲区
     * 2. 在缓冲区中检查是否有完整消息
     * 3. 如果有完整消息，解析并移除
     * 4. 返回接收到的消息数量
     *
     * @param[out] message 存储接收到的消息
     * @return true 如果接收到至少一条完整的消息
     *
     * @note 该函数应在专用线程中循环调用
     * @note 连接断开时会返回false
     * @note 可能一次调用接收多条消息（但只返回一条）
     */
    bool receive_message(Message& message) {
        if (!connected_.load()) {
            return false;
        }

        // 从套接字接收数据
        uint8_t recv_buf[4096];
        int bytes_received = ::recv(socket_, reinterpret_cast<char*>(recv_buf),
                                   sizeof(recv_buf), 0);

        if (bytes_received <= 0) {
            // 连接断开或错误
            connected_ = false;
            return false;
        }

        // 更新最后活动时间
        last_activity_time_ = std::chrono::steady_clock::now();

        // 写入循环缓冲区
        size_t written = recv_buffer_.write(recv_buf, bytes_received);
        if (written != static_cast<size_t>(bytes_received)) {
            std::cerr << "Warning: recv_buffer full, dropping data" << std::endl;
        }

        // 尝试从缓冲区提取完整消息
        if (try_extract_message(message)) {
            return true;
        }

        return false;
    }

    /**
     * @brief 接收消息（阻塞版本，带超时）
     *
     * @param[out] message 存储接收到的消息
     * @param timeout_ms 超时时间（毫秒）
     * @return true 如果在超时内接收到消息
     *
     * @note 如果timeout_ms为0，等价于receive_message()
     * @note 该函数在循环中调用，直到接收到消息或超时
     */
    bool receive_message_with_timeout(Message& message, int timeout_ms) {
        auto start_time = std::chrono::steady_clock::now();

        while (connected_.load()) {
            if (receive_message(message)) {
                return true;
            }

            // 检查超时
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms) {
                return false;
            }

            // 短暂睡眠以避免busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return false;
    }

    // ===== 消息发送 =====

    /**
     * @brief 发送消息
     *
     * @param[in] message 要发送的消息
     * @return true 如果消息成功发送
     *
     * @note 消息会被序列化并立即发送
     * @note 如果连接已断开，返回false
     * @note 该函数是同步的，可能阻塞在发送操作上
     */
    bool send(const Message& message) {
        if (!connected_.load()) {
            return false;
        }

        // 序列化消息为字节流
        std::vector<uint8_t> bytes = message.to_bytes();

        // 发送数据
        int total_sent = 0;
        int remaining = bytes.size();
        const uint8_t* data = bytes.data();

        while (remaining > 0) {
            int sent = ::send(socket_, reinterpret_cast<const char*>(data + total_sent),
                            remaining, 0);

            if (sent <= 0) {
                // 发送失败，连接可能断开
                connected_ = false;
                std::cerr << "Failed to send message on connection #" << id_ << std::endl;
                return false;
            }

            total_sent += sent;
            remaining -= sent;
        }

        // 更新最后活动时间
        last_activity_time_ = std::chrono::steady_clock::now();

        return true;
    }

    /**
     * @brief 发送心跳包
     *
     * @return true 如果心跳发送成功
     *
     * @note 心跳是一个空消息体的HEARTBEAT消息
     * @note 用于检测连接是否仍然活跃
     */
    bool send_heartbeat() {
        Message heartbeat(MessageType::HEARTBEAT, 0, ProtocolHelper::get_timestamp_ms());
        return send(heartbeat);
    }

    /**
     * @brief 发送心跳确认
     *
     * @return true 如果心跳确认发送成功
     */
    bool send_heartbeat_ack() {
        Message heartbeat_ack(MessageType::HEARTBEAT_ACK, 0,
                            ProtocolHelper::get_timestamp_ms());
        return send(heartbeat_ack);
    }

    // ===== 时间管理 =====

    /**
     * @brief 获取连接的最后活动时间
     *
     * @return 最后活动的时间点
     *
     * @note 用于检测僵尸连接（无心跳响应）
     */
    std::chrono::steady_clock::time_point get_last_activity_time() const {
        return last_activity_time_;
    }

    /**
     * @brief 检查连接是否超时
     *
     * @param timeout_ms 超时阈值（毫秒）
     * @return true 如果距离最后活动超过timeout_ms
     */
    bool is_timeout(int timeout_ms) const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_activity_time_).count();
        return elapsed > timeout_ms;
    }

    /**
     * @brief 获取缓冲区统计信息
     *
     * @return 一对值（接收缓冲区中可用数据, 接收缓冲区剩余空间）
     */
    std::pair<size_t, size_t> get_buffer_stats() const {
        return {recv_buffer_.available_data(), recv_buffer_.available_space()};
    }

private:
    /**
     * @brief 尝试从接收缓冲区提取完整消息
     *
     * @param[out] message 存储提取的消息
     * @return true 如果成功提取一条完整消息
     *
     * 步骤：
     * 1. Peek（查看）循环缓冲区中的数据
     * 2. 检查是否有足够的数据用于消息头
     * 3. 解析消息头，获取消息体大小
     * 4. 检查是否有完整的消息（头+体）
     * 5. 如果有，读取并从缓冲区中移除
     * 6. 验证消息有效性
     *
     * @note 该函数不修改缓冲区读指针（除非成功提取）
     */
    bool try_extract_message(Message& message) {
        // 检查缓冲区中是否有足够的数据用于消息头
        if (recv_buffer_.available_data() < MessageHeader::HEADER_SIZE) {
            return false;
        }

        // Peek消息头
        std::vector<uint8_t> header_buf(MessageHeader::HEADER_SIZE);
        size_t peeked = recv_buffer_.peek(header_buf.data(), MessageHeader::HEADER_SIZE);
        if (peeked != MessageHeader::HEADER_SIZE) {
            return false;
        }

        // 解析消息头
        MessageHeader header;
        header.deserialize(header_buf.data());

        // 验证消息头
        if (!header.is_valid()) {
            std::cerr << "Invalid message header on connection #" << id_ << std::endl;
            // 清空缓冲区以恢复同步
            recv_buffer_.clear();
            return false;
        }

        // 检查是否有完整的消息（头+体）
        size_t total_needed = MessageHeader::HEADER_SIZE + header.payload_size;
        if (recv_buffer_.available_data() < total_needed) {
            // 消息还不完整，等待更多数据
            return false;
        }

        // 读取完整消息
        std::vector<uint8_t> msg_data(total_needed);
        size_t read_size = recv_buffer_.read(msg_data.data(), total_needed);
        if (read_size != total_needed) {
            std::cerr << "Failed to read complete message on connection #" << id_
                     << std::endl;
            return false;
        }

        // 反序列化消息
        if (!message.from_bytes(msg_data)) {
            std::cerr << "Failed to deserialize message on connection #" << id_
                     << std::endl;
            return false;
        }

        return true;
    }

private:
    // 连接基本信息
    uint32_t id_;                                   // 连接ID
    SOCKET socket_;                                 // TCP套接字
    struct sockaddr_in client_addr_;                // 客户端地址
    std::string client_addr_str_;                   // 客户端地址字符串
    const struct ServerConfig& config_;             // 服务器配置引用

    // 连接状态
    std::atomic<bool> connected_;                   // 连接状态标志

    // 接收数据缓冲
    CircularBuffer recv_buffer_;                    // 循环缓冲区用于接收数据

    // 时间管理
    std::chrono::steady_clock::time_point last_activity_time_;  // 最后活动时间
};

#endif // CONNECTION_H
