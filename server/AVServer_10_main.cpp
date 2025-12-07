/*
 * main.cpp - 音视频服务器应用程序入口点
 *
 * 功能：
 * - 初始化服务器配置
 * - 启动服务器
 * - 处理信号和用户交互
 * - 优雅关闭
 *
 * 编译方式：
 * Linux/Unix:
 *   g++ -std=c++17 -pthread -o avserver main.cpp
 *   clang++ -std=c++17 -pthread -o avserver main.cpp
 *
 * Windows:
 *   需要链接 ws2_32.lib（已在TcpServer.h中设置）
 *
 * 运行方式：
 *   ./avserver
 *   # 或指定端口
 *   ./avserver 9999
 *
 * 交互命令：
 *   help     - 显示帮助信息
 *   status   - 显示服务器状态
 *   stats    - 显示统计信息
 *   conns    - 显示当前连接数
 *   quit/exit - 优雅关闭服务器
 */

#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <csignal>
#include <atomic>
#include <memory>

#include "AVServer_09_AVServer.h"

// ============================================================================
// ======================== 全局变量 ===========================================
// ============================================================================

// 全局服务器实例（用于信号处理）
std::unique_ptr<AVServer> g_server = nullptr;

// 信号处理标志
std::atomic<bool> g_shutdown_requested(false);

// ============================================================================
// ======================== 信号处理 ===========================================
// ============================================================================

/**
 * @brief 信号处理函数
 *
 * 处理的信号：
 * - SIGINT (Ctrl+C)
 * - SIGTERM (终止信号)
 *
 * @note 设置关闭标志，触发优雅退出
 */
void signal_handler(int sig) {
    std::cout << "\n[SIGNAL] Received signal " << sig << ", initiating shutdown..." << std::endl;
    g_shutdown_requested = true;
}

// ============================================================================
// ======================== 命令处理 ===========================================
// ============================================================================

/**
 * @brief 显示帮助信息
 */
void show_help() {
    std::cout << "\n=== AVServer Commands ===" << std::endl;
    std::cout << "help       - Show this help message" << std::endl;
    std::cout << "status     - Show server status (running/stopped)" << std::endl;
    std::cout << "stats      - Show server statistics" << std::endl;
    std::cout << "conns      - Show current connection count" << std::endl;
    std::cout << "fullstats  - Show comprehensive statistics (all modules)" << std::endl;
    std::cout << "quit/exit  - Shutdown server gracefully" << std::endl;
    std::cout << "clear      - Clear screen" << std::endl;
    std::cout << "" << std::endl;
}

/**
 * @brief 处理用户输入的命令
 *
 * @param command 用户输入的命令字符串
 * @return true 如果应该继续运行，false 如果应该退出
 */
bool process_command(const std::string& command) {
    if (command.empty()) {
        return true;
    }

    // 将命令转换为小写
    std::string cmd = command;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    // 移除前后空格
    cmd.erase(0, cmd.find_first_not_of(" \t"));
    cmd.erase(cmd.find_last_not_of(" \t") + 1);

    if (cmd == "help") {
        show_help();
    }
    else if (cmd == "status") {
        if (g_server && g_server->is_running()) {
            std::cout << "[STATUS] Server is RUNNING" << std::endl;
        } else {
            std::cout << "[STATUS] Server is STOPPED" << std::endl;
        }
    }
    else if (cmd == "stats") {
        if (g_server) {
            std::cout << "\n";
            g_server->print_statistics();
            std::cout << "" << std::endl;
        }
    }
    else if (cmd == "fullstats") {
        if (g_server) {
            std::cout << "\n";
            g_server->print_comprehensive_statistics();
            std::cout << "" << std::endl;
        }
    }
    else if (cmd == "conns") {
        if (g_server) {
            size_t conns = g_server->get_tcp_server().get_connection_count();
            std::cout << "[CONNS] Current connections: " << conns << std::endl;
        }
    }
    else if (cmd == "quit" || cmd == "exit") {
        std::cout << "[QUIT] Shutting down server..." << std::endl;
        return false;
    }
    else if (cmd == "clear") {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }
    else {
        std::cout << "[ERROR] Unknown command: " << command << std::endl;
        std::cout << "[INFO] Type 'help' for available commands" << std::endl;
    }

    return true;
}

// ============================================================================
// ======================== 命令行界面 ========================================
// ============================================================================

/**
 * @brief 命令行交互循环
 *
 * 提供交互式命令界面，用户可以：
 * - 查看服务器状态
 * - 显示统计信息
 * - 控制服务器运行
 * - 获取帮助信息
 *
 * @note 在单独的线程中运行，不阻塞服务器主循环
 */
void command_loop() {
    std::string command;

    show_help();

    while (g_server && g_server->is_running() && !g_shutdown_requested.load()) {
        std::cout << "> ";
        std::cout.flush();

        if (!std::getline(std::cin, command)) {
            // 输入流出错或关闭
            break;
        }

        if (!process_command(command)) {
            g_shutdown_requested = true;
            break;
        }
    }
}

// ============================================================================
// ======================== 主程序 ============================================
// ============================================================================

/**
 * @brief 主程序入口点
 *
 * 功能：
 * 1. 解析命令行参数
 * 2. 创建服务器配置
 * 3. 初始化AVServer实例
 * 4. 注册信号处理器
 * 5. 启动服务器
 * 6. 启动命令交互线程
 * 7. 等待退出信号
 * 8. 优雅关闭
 *
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 0 如果正常退出，1 如果出错
 *
 * 使用示例：
 *   avserver                    # 使用默认配置（端口8888）
 *   avserver 9999              # 使用自定义端口
 *   avserver --port 9999       # 使用--port参数指定端口
 */
int main(int argc, char* argv[]) {
    std::cout << "=== AVServer - Audio/Video Server ===" << std::endl;
    std::cout << "Version: 1.0" << std::endl;
    std::cout << "Built with: C++17" << std::endl;
    std::cout << "" << std::endl;

    // ===== 1. 解析命令行参数 =====
    ServerConfig config;

    // 检查端口参数
    if (argc >= 2) {
        std::string arg = argv[1];

        // 检查是否是--port参数
        if (arg == "--port" && argc >= 3) {
            try {
                config.port = std::stoi(argv[2]);
                std::cout << "[CONFIG] Port set to: " << config.port << std::endl;
            } catch (...) {
                std::cerr << "[ERROR] Invalid port number: " << argv[2] << std::endl;
                return 1;
            }
        }
        // 或直接指定端口号
        else if (arg[0] != '-') {
            try {
                config.port = std::stoi(arg);
                std::cout << "[CONFIG] Port set to: " << config.port << std::endl;
            } catch (...) {
                std::cerr << "[ERROR] Invalid port number: " << arg << std::endl;
                return 1;
            }
        }
    }

    // ===== 2. 显示配置信息 =====
    std::cout << "[CONFIG] Server Configuration:" << std::endl;
    std::cout << "  Listen Address: " << config.listen_addr << std::endl;
    std::cout << "  Listen Port: " << config.port << std::endl;
    std::cout << "  Max Connections: " << config.max_connections << std::endl;
    std::cout << "  Thread Pool Size: " << config.thread_pool_size << std::endl;
    std::cout << "  Recv Buffer: " << (config.recv_buffer_size / 1024) << " KB" << std::endl;
    std::cout << "  Send Buffer: " << (config.send_buffer_size / 1024) << " KB" << std::endl;
    std::cout << "" << std::endl;

    // ===== 3. 创建服务器实例 =====
    g_server = std::make_unique<AVServer>(config);

    // ===== 4. 注册信号处理器 =====
    // 处理Ctrl+C和终止信号
    std::signal(SIGINT, signal_handler);
    #ifndef _WIN32
        std::signal(SIGTERM, signal_handler);
    #endif

    // ===== 5. 启动服务器 =====
    std::cout << "[STARTUP] Starting server..." << std::endl;
    if (!g_server->start()) {
        std::cerr << "[ERROR] Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "[STARTUP] Server started successfully" << std::endl;
    std::cout << "[STARTUP] Listening on " << config.listen_addr << ":"
              << config.port << std::endl;
    std::cout << "" << std::endl;

    // ===== 6. 启动命令交互线程 =====
    // 在单独的线程中运行命令循环，允许用户交互
    std::thread cmd_thread(command_loop);

    // ===== 7. 等待退出信号 =====
    // 主线程定期检查关闭标志
    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 可以在这里添加定期任务，例如：
        // - 检测僵尸连接
        // - 清理过期资源
        // - 性能监控
        // - 自动备份
    }

    // ===== 8. 优雅关闭 =====
    std::cout << "\n[SHUTDOWN] Initiating graceful shutdown..." << std::endl;

    // 停止服务器
    if (g_server) {
        g_server->stop();
    }

    // 等待命令线程退出
    if (cmd_thread.joinable()) {
        cmd_thread.join();
    }

    std::cout << "[SHUTDOWN] Server shutdown complete" << std::endl;
    std::cout << "[SHUTDOWN] Goodbye!" << std::endl;

    return 0;
}

// ============================================================================
// ======================== 编译和运行说明 ====================================
// ============================================================================

/*
编译说明：

Linux/Unix (GCC/Clang):
    # 基本编译
    g++ -std=c++17 -pthread -o avserver main.cpp

    # 优化编译
    g++ -std=c++17 -pthread -O2 -o avserver main.cpp

    # 调试编译
    g++ -std=c++17 -pthread -g -O0 -o avserver main.cpp

Windows (Visual Studio):
    # 在Visual Studio中创建项目，添加所有.h文件到项目中
    # 确保项目配置为C++17或更新
    # 编译时会自动链接ws2_32.lib

MacOS (Clang):
    # 与Linux类似
    clang++ -std=c++17 -pthread -o avserver main.cpp

运行说明：

    # 默认配置（端口8888）
    ./avserver

    # 指定端口
    ./avserver 9999

    # 后台运行（Linux）
    ./avserver &

    # 重定向输出到日志文件
    ./avserver > server.log 2>&1 &

客户端连接示例：

    # 使用netcat测试连接
    nc localhost 8888

    # 使用telnet测试
    telnet localhost 8888

    # 使用C++客户端代码
    #include <arpa/inet.h>
    #include <sys/socket.h>
    ...

调试技巧：

    1. 启用详细日志
       - 修改代码中的std::cout语句
       - 添加DEBUG宏
       - 使用外部日志库（如spdlog）

    2. 使用gdb调试
       g++ -g -o avserver main.cpp
       gdb ./avserver
       (gdb) run
       (gdb) bt      # 打印调用栈
       (gdb) print variable

    3. 性能分析
       # 使用perf工具（Linux）
       perf record -g ./avserver
       perf report

    4. 内存检查
       # 使用valgrind
       valgrind --leak-check=full ./avserver

生产部署建议：

    1. 使用systemd服务
       # 创建 /etc/systemd/system/avserver.service
       [Unit]
       Description=Audio/Video Server
       After=network.target

       [Service]
       Type=simple
       User=nobody
       WorkingDirectory=/opt/avserver
       ExecStart=/opt/avserver/avserver 8888
       Restart=always
       RestartSec=5

       [Install]
       WantedBy=multi-user.target

       # 启动服务
       systemctl start avserver
       systemctl enable avserver

    2. 配置防火墙
       # iptables (Linux)
       iptables -A INPUT -p tcp --dport 8888 -j ACCEPT

    3. 监控和日志
       # 重定向到日志文件
       ./avserver > /var/log/avserver.log 2>&1 &

    4. 性能调优
       # 增加文件描述符限制
       ulimit -n 65536

       # 调整网络参数
       sysctl -w net.core.rmem_max=134217728
       sysctl -w net.core.wmem_max=134217728
 */
