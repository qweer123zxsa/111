/*
 * TcpServer.h - TCP服务器实现
 *
 * 功能：
 * - 管理TCP服务器的生命周期
 * - 监听客户端连接请求
 * - 接受新连接并分配给工作线程
 * - 管理所有活跃的客户端连接
 *
 * 设计模式：
 * - Reactor模式：事件驱动的网络编程
 * - 线程池管理：使用ThreadPool处理客户端请求
 * - 连接生命周期管理：自动清理断开的连接
 *
 * 应用场景：
 * - 多客户端并发连接
 * - 实时音视频流服务
 * - 高吞吐量网络应用
 *
 * 平台支持：
 * - 仅在Linux/Unix平台上使用（使用POSIX套接字）
 * - 在Windows上需要修改为Winsock API
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <thread>
#include <memory>
#include <atomic>
#include <vector>
#include <map>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstring>
#include <iostream>

// 网络相关的头文件
#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    typedef int SOCKET;
#endif

#include "AVServer_01_SafeQueue.h"
#include "AVServer_04_ThreadPool.h"

// ============================================================================
// ======================== TCP服务器配置 ======================================
// ============================================================================

/**
 * @struct ServerConfig
 * @brief TCP服务器的配置参数
 *
 * 包含所有影响服务器行为的参数：
 * - 监听地址和端口
 * - 最大连接数
 * - 接收/发送缓冲区大小
 * - 超时设置
 * - 线程数量
 */
struct ServerConfig {
    uint16_t port;                  // 监听端口（默认8888）
    std::string listen_addr;        // 监听地址（默认0.0.0.0表示所有接口）
    int max_connections;            // 最大并发连接数（默认1000）
    int listen_backlog;             // TCP监听队列长度（默认128）

    int recv_buffer_size;           // 接收缓冲区大小（默认256KB）
    int send_buffer_size;           // 发送缓冲区大小（默认256KB）

    int recv_timeout_ms;            // 接收超时（毫秒，0表示无限制）
    int send_timeout_ms;            // 发送超时（毫秒，0表示无限制）

    int heartbeat_interval_ms;      // 心跳包间隔（毫秒）
    int heartbeat_timeout_ms;       // 心跳超时阈值（毫秒）

    size_t thread_pool_size;        // 处理客户端请求的线程数

    /**
     * @brief 构造函数 - 初始化为默认值
     */
    ServerConfig()
        : port(8888),
          listen_addr("0.0.0.0"),
          max_connections(1000),
          listen_backlog(128),
          recv_buffer_size(256 * 1024),      // 256KB
          send_buffer_size(256 * 1024),      // 256KB
          recv_timeout_ms(0),                // 无限制
          send_timeout_ms(0),                // 无限制
          heartbeat_interval_ms(5000),       // 5秒
          heartbeat_timeout_ms(15000),       // 15秒
          thread_pool_size(4) {              // 4个工作线程
    }
};

// ============================================================================
// ======================== TCP服务器类 =======================================
// ============================================================================

/**
 * @class TcpServer
 * @brief 多线程TCP服务器
 *
 * 工作流程：
 * 1. 创建并绑定监听套接字到指定端口
 * 2. 启动接收线程，等待客户端连接
 * 3. 接受新连接，创建Connection对象管理
 * 4. 分配给线程池处理客户端请求
 * 5. 监测连接状态，断开时进行清理
 *
 * 使用示例：
 * @code
 *   ServerConfig config;
 *   config.port = 8888;
 *   config.thread_pool_size = 8;
 *
 *   TcpServer server(config);
 *
 *   // 设置回调函数
 *   server.on_client_connected = [](const std::shared_ptr<Connection>& conn) {
 *       std::cout << "Client connected: " << conn->get_addr() << std::endl;
 *   };
 *
 *   server.on_message_received = [](const std::shared_ptr<Connection>& conn,
 *                                   const Message& msg) {
 *       std::cout << "Received: " << msg.to_string() << std::endl;
 *   };
 *
 *   // 启动服务器
 *   if (!server.start()) {
 *       std::cerr << "Failed to start server" << std::endl;
 *       return 1;
 *   }
 *
 *   // 服务器运行...
 *
 *   // 优雅关闭
 *   server.stop();
 * @endcode
 *
 * @note 该类使用POSIX套接字，仅支持Linux/Unix平台
 * @note 使用thread_pool_处理客户端请求，避免主线程阻塞
 */
class TcpServer {
public:
    /**
     * @brief 客户端连接事件回调
     * 参数：指向Connection对象的智能指针
     *
     * @note 在客户端连接时被调用
     * @note 可以在此初始化客户端相关状态
     */
    using OnClientConnectedCallback = std::function<void(const std::shared_ptr<class Connection>&)>;

    /**
     * @brief 接收消息事件回调
     * 参数：指向Connection对象的智能指针，接收到的Message
     *
     * @note 在收到完整消息时被调用
     * @note 在线程池线程中执行，需要考虑线程安全
     */
    using OnMessageReceivedCallback = std::function<void(
        const std::shared_ptr<class Connection>&,
        const Message&)>;

    /**
     * @brief 客户端断开连接事件回调
     * 参数：指向Connection对象的智能指针
     *
     * @note 在客户端连接断开时被调用
     * @note 可以进行连接清理和资源释放
     */
    using OnClientDisconnectedCallback = std::function<void(const std::shared_ptr<class Connection>&)>;

    /**
     * @brief 构造函数
     *
     * @param config 服务器配置参数
     *
     * @note 构造函数不启动服务器，需要显式调用start()
     */
    explicit TcpServer(const ServerConfig& config = ServerConfig())
        : config_(config),
          listen_socket_(INVALID_SOCKET),
          running_(false),
          accept_thread_(),
          thread_pool_(config.thread_pool_size),
          next_connection_id_(1) {
    }

    /**
     * @brief 析构函数
     * 自动关闭服务器和清理资源
     */
    ~TcpServer() {
        stop();
    }

    /**
     * @brief 启动TCP服务器
     *
     * @return true 如果服务器启动成功
     *
     * 步骤：
     * 1. 创建监听套接字
     * 2. 绑定到指定地址和端口
     * 3. 开始监听客户端连接
     * 4. 启动接收线程
     *
     * @note 如果端口已被占用，返回false
     * @note 该函数阻塞直到服务器成功启动或出错
     */
    bool start() {
        // 如果已经启动，直接返回
        if (running_.load()) {
            return true;
        }

        // 创建TCP监听套接字
        listen_socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_socket_ == INVALID_SOCKET) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        // 设置套接字选项
        // SO_REUSEADDR：允许重新使用TIME_WAIT状态的端口
        int reuse_addr = 1;
        if (::setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR,
                        reinterpret_cast<const char*>(&reuse_addr),
                        sizeof(reuse_addr)) < 0) {
            std::cerr << "Failed to set SO_REUSEADDR" << std::endl;
            ::closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
            return false;
        }

        // 设置缓冲区大小
        if (::setsockopt(listen_socket_, SOL_SOCKET, SO_RCVBUF,
                        reinterpret_cast<const char*>(&config_.recv_buffer_size),
                        sizeof(config_.recv_buffer_size)) < 0) {
            std::cerr << "Failed to set SO_RCVBUF" << std::endl;
        }

        if (::setsockopt(listen_socket_, SOL_SOCKET, SO_SNDBUF,
                        reinterpret_cast<const char*>(&config_.send_buffer_size),
                        sizeof(config_.send_buffer_size)) < 0) {
            std::cerr << "Failed to set SO_SNDBUF" << std::endl;
        }

        // 构建绑定地址
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = ::htons(config_.port);
        server_addr.sin_addr.s_addr = ::inet_addr(config_.listen_addr.c_str());

        // 如果是0.0.0.0，则绑定到所有接口
        if (config_.listen_addr == "0.0.0.0") {
            server_addr.sin_addr.s_addr = INADDR_ANY;
        }

        // 绑定套接字
        if (::bind(listen_socket_, reinterpret_cast<struct sockaddr*>(&server_addr),
                  sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Failed to bind to port " << config_.port << std::endl;
            ::closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
            return false;
        }

        // 监听连接请求
        if (::listen(listen_socket_, config_.listen_backlog) == SOCKET_ERROR) {
            std::cerr << "Failed to listen on socket" << std::endl;
            ::closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
            return false;
        }

        // 设置标志并启动接收线程
        running_ = true;
        accept_thread_ = std::thread(&TcpServer::accept_loop, this);

        std::cout << "Server started on " << config_.listen_addr << ":"
                  << config_.port << std::endl;
        return true;
    }

    /**
     * @brief 停止TCP服务器
     *
     * 步骤：
     * 1. 设置running_标志为false
     * 2. 关闭监听套接字（使accept()返回）
     * 3. 等待接收线程退出
     * 4. 关闭所有客户端连接
     * 5. 关闭线程池
     *
     * @note 函数会阻塞直到所有连接都关闭
     * @note 可以安全地多次调用
     */
    void stop() {
        // 设置停止标志
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {
            // 已经停止
            return;
        }

        // 关闭监听套接字，导致accept()返回
        if (listen_socket_ != INVALID_SOCKET) {
            ::closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
        }

        // 等待接收线程退出
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }

        // 关闭所有客户端连接
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            for (auto& [id, conn] : connections_) {
                if (conn) {
                    conn->close();
                }
            }
            connections_.clear();
        }

        // 关闭线程池
        thread_pool_.shutdown();

        std::cout << "Server stopped" << std::endl;
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
     * @brief 获取当前连接的客户端数量
     *
     * @return 活跃连接的数量
     */
    size_t get_connection_count() const {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        return connections_.size();
    }

    /**
     * @brief 获取指定ID的连接
     *
     * @param connection_id 连接ID
     * @return 指向Connection的智能指针，如果不存在返回nullptr
     */
    std::shared_ptr<class Connection> get_connection(uint32_t connection_id) const {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief 向所有连接广播消息
     *
     * @param[in] message 要发送的消息
     *
     * @note 向所有活跃连接发送相同的消息
     * @note 如果某个连接发送失败，继续发送给其他连接
     */
    void broadcast(const Message& message) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& [id, conn] : connections_) {
            if (conn && conn->is_connected()) {
                conn->send(message);
            }
        }
    }

    /**
     * @brief 设置客户端连接事件回调
     *
     * @param callback 回调函数
     *
     * @note 该回调在new_connection()方法中被调用
     * @note 在主线程中执行
     */
    void set_on_client_connected(OnClientConnectedCallback callback) {
        on_client_connected_ = callback;
    }

    /**
     * @brief 设置接收消息事件回调
     *
     * @param callback 回调函数
     *
     * @note 该回调在线程池线程中执行
     * @note 需要考虑线程安全性
     */
    void set_on_message_received(OnMessageReceivedCallback callback) {
        on_message_received_ = callback;
    }

    /**
     * @brief 设置客户端断开连接事件回调
     *
     * @param callback 回调函数
     */
    void set_on_client_disconnected(OnClientDisconnectedCallback callback) {
        on_client_disconnected_ = callback;
    }

    /**
     * @brief 获取服务器配置
     *
     * @return 常量引用到服务器配置
     */
    const ServerConfig& get_config() const {
        return config_;
    }

private:
    /**
     * @brief 接收线程的主循环
     *
     * 功能：
     * 1. 循环调用accept()等待新连接
     * 2. 创建Connection对象包装新套接字
     * 3. 分配connection_id
     * 4. 调用on_client_connected_回调
     * 5. 分配给线程池进行处理
     *
     * @note 该函数运行在独立的线程中
     * @note 当running_标志为false时函数返回
     */
    void accept_loop() {
        while (running_.load()) {
            // 接受新连接
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            SOCKET client_socket = ::accept(listen_socket_,
                                          reinterpret_cast<struct sockaddr*>(&client_addr),
                                          &addr_len);

            if (client_socket == INVALID_SOCKET) {
                // 如果accept()因为关闭而失败，直接退出
                if (!running_.load()) {
                    break;
                }
                // 其他错误情况
                std::cerr << "accept() failed" << std::endl;
                continue;
            }

            // 创建Connection对象
            uint32_t connection_id = next_connection_id_++;
            auto connection = std::make_shared<class Connection>(
                connection_id,
                client_socket,
                client_addr,
                config_);

            // 保存到连接映射表
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                if (connections_.size() < static_cast<size_t>(config_.max_connections)) {
                    connections_[connection_id] = connection;
                } else {
                    // 达到最大连接数，拒绝新连接
                    std::cerr << "Max connections reached, rejecting new connection" << std::endl;
                    connection->close();
                    continue;
                }
            }

            // 调用连接回调
            if (on_client_connected_) {
                on_client_connected_(connection);
            }

            // 分配给线程池处理
            // 捕获connection的弱指针，防止过期
            auto weak_conn = std::weak_ptr<class Connection>(connection);
            thread_pool_.add_work([this, weak_conn]() {
                auto conn = weak_conn.lock();
                if (!conn) {
                    return;  // 连接已断开
                }
                handle_client(conn);
            });
        }
    }

    /**
     * @brief 处理单个客户端的消息接收和处理
     *
     * @param connection 指向客户端Connection对象的智能指针
     *
     * @note 该函数在线程池线程中执行
     * @note 持续接收消息直到连接断开
     */
    void handle_client(const std::shared_ptr<class Connection>& connection) {
        // TODO: 这是一个占位符，具体实现在Connection类中
        // 实际实现应该在Connection类中处理消息接收
        if (on_client_disconnected_) {
            on_client_disconnected_(connection);
        }

        // 从连接映射表中移除
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.erase(connection->get_id());
        }
    }

    /**
     * @brief 被Connection调用，用于处理接收到的消息
     *
     * @param connection 发送消息的连接
     * @param message 接收到的消息
     *
     * @note 该函数在处理客户端的线程中执行
     * @note 需要考虑线程安全性
     */
    void on_message_received(const std::shared_ptr<class Connection>& connection,
                            const Message& message) {
        if (on_message_received_) {
            on_message_received_(connection, message);
        }
    }

    // 友元声明，允许Connection访问私有方法
    friend class Connection;

private:
    ServerConfig config_;                           // 服务器配置

    SOCKET listen_socket_;                          // 监听套接字
    std::atomic<bool> running_;                     // 运行标志

    std::thread accept_thread_;                     // 接收连接的线程
    ThreadPool thread_pool_;                        // 处理客户端请求的线程池

    mutable std::mutex connections_mutex_;          // 保护connections_的互斥锁
    std::map<uint32_t, std::shared_ptr<class Connection>> connections_;  // 活跃连接映射表
    std::atomic<uint32_t> next_connection_id_;      // 下一个连接ID

    // 事件回调函数
    OnClientConnectedCallback on_client_connected_;
    OnMessageReceivedCallback on_message_received_;
    OnClientDisconnectedCallback on_client_disconnected_;
};

#endif // TCP_SERVER_H
