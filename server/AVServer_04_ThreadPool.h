/*
 * ThreadPool.h - 高效的线程池实现
 *
 * 功能：
 * - 管理一组工作线程
 * - 分发任务到可用线程
 * - 自动负载均衡
 * - 优雅关闭
 *
 * 设计模式：
 * 使用线程池可以：
 * - 避免频繁创建销毁线程的开销
 * - 提高系统的整体响应性能
 * - 有效控制并发线程数量
 *
 * 应用场景：
 * - Web服务器（处理HTTP请求）
 * - 游戏服务器（处理玩家操作）
 * - 数据库服务器（处理查询）
 * - 本项目中处理客户端连接
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include "SafeQueue.h"

/**
 * @class ThreadPool
 * @brief 通用线程池实现
 *
 * 工作原理：
 * 1. 创建时启动num_threads个工作线程
 * 2. 工作线程持续从任务队列中获取任务
 * 3. 获取到任务后执行，然后继续等待下一个任务
 * 4. 关闭时发送特殊信号让所有线程优雅退出
 *
 * 使用示例：
 * @code
 *   // 创建包含4个工作线程的线程池
 *   ThreadPool pool(4);
 *
 *   // 添加任务
 *   pool.add_task([]() {
 *       std::cout << "Task 1 executed" << std::endl;
 *   });
 *
 *   pool.add_task([]() {
 *       std::cout << "Task 2 executed" << std::endl;
 *   });
 *
 *   // 析构时自动等待所有任务完成并关闭线程
 * @endcode
 */
class ThreadPool {
public:
    /**
     * @brief 任务类型定义
     * std::function是函数包装器，可以存储任何可调用对象
     * 包括：函数指针、lambda、std::bind结果、函数对象等
     */
    using Task = std::function<void()>;

    /**
     * @brief 构造函数 - 创建线程池
     *
     * @param num_threads 线程数量
     *        - 通常设为CPU核心数或稍多一些
     *        - 推荐：std::thread::hardware_concurrency()
     *        - 本项目中推荐值：4-16，根据服务器配置调整
     *
     * @note 构造后线程立即启动并等待任务
     */
    explicit ThreadPool(size_t num_threads = 4)
        : stop_(false),
          active_tasks_(0) {
        // 创建num_threads个工作线程
        for (size_t i = 0; i < num_threads; ++i) {
            // std::thread会立即启动线程
            // &ThreadPool::worker是成员函数指针
            // this是传给成员函数的隐式参数
            threads_.emplace_back(&ThreadPool::worker, this);
        }
    }

    /**
     * @brief 析构函数 - 优雅关闭线程池
     *
     * 关闭过程：
     * 1. 设置stop_标志为true
     * 2. 通知所有等待的线程
     * 3. 等待所有线程的worker函数返回
     * 4. join所有线程
     *
     * @note 会等待所有正在执行的任务完成
     * @note 但会立即丢弃队列中未开始的任务
     */
    ~ThreadPool() {
        shutdown();
    }

    /**
     * @brief 向线程池添加任务
     *
     * @tparam F 任务函数的类型
     * @tparam Args 任务函数参数的类型
     * @param f 任务函数
     * @param args 任务函数的参数
     *
     * @return 返回一个std::future对象，可用于获取任务的返回值
     *
     * 支持各种形式的任务：
     * @code
     *   // 1. Lambda表达式
     *   pool.add_task([]() { std::cout << "Task 1" << std::endl; });
     *
     *   // 2. 带参数的lambda
     *   pool.add_task([](int x) { return x * 2; }, 5);
     *
     *   // 3. 函数指针
     *   void my_func() { }
     *   pool.add_task(my_func);
     *
     *   // 4. 成员函数
     *   MyClass obj;
     *   pool.add_task(&MyClass::method, &obj);
     * @endcode
     *
     * @note 如果线程池已关闭，任务不会被添加
     */
    template <typename F, typename... Args>
    auto add_task(F&& f, Args&&... args) {
        // 类型别名：提取函数返回值类型
        // std::invoke_result_t<F, Args...> 表示 f(args...)的返回值类型
        using return_type = typename std::invoke_result_t<F, Args...>;

        // 使用std::bind将函数和参数绑定
        // std::forward完美转发，保留参数的左值/右值属性
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        // 创建future对象，用于获取结果
        auto future = task->get_future();

        // 添加任务到队列
        // Lambda将std::packaged_task转换为std::function<void()>
        queue_.push([task]() { (*task)(); });

        return future;
    }

    /**
     * @brief 添加不需要返回值的任务（简化版）
     *
     * @tparam F 任务函数的类型
     * @tparam Args 参数类型
     * @param f 任务函数
     * @param args 参数
     *
     * 使用场景：当任务不需要返回值时，这个方法更简洁
     */
    template <typename F, typename... Args>
    void add_work(F&& f, Args&&... args) {
        queue_.push(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
    }

    /**
     * @brief 获取当前队列中待处理的任务数
     *
     * @return 任务队列中的任务数
     *
     * @note 这个数值可能在返回后立即改变
     * @note 主要用于监控和调试，不应用于逻辑判断
     */
    size_t queue_size() const {
        return queue_.size();
    }

    /**
     * @brief 获取当前正在执行的任务数
     *
     * @return 活跃任务的数量
     *
     * @note 活跃任务数 = 正在执行的任务 + 队列中的任务
     */
    size_t active_tasks() const {
        return active_tasks_.load();
    }

    /**
     * @brief 获取线程池中的线程数
     *
     * @return 工作线程的数量
     */
    size_t thread_count() const {
        return threads_.size();
    }

    /**
     * @brief 关闭线程池
     *
     * 过程：
     * 1. 设置stop标志
     * 2. 唤醒所有等待的线程
     * 3. 等待所有线程退出
     *
     * @note 会等待所有正在执行的任务完成
     * @note 可以多次调用，第一次调用后的调用无效果
     */
    void shutdown() {
        // 原子操作，避免竞态条件
        bool expected = false;
        if (stop_.compare_exchange_strong(expected, true)) {
            // 设置成功，说明这是第一次调用shutdown

            // 关闭队列，让所有等待的pop立即返回
            queue_.shutdown();

            // 等待所有线程退出
            for (auto& thread : threads_) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
        }
    }

    /**
     * @brief 检查线程池是否已关闭
     *
     * @return true如果线程池已关闭，false如果仍在运行
     */
    bool is_shutdown() const {
        return stop_.load();
    }

private:
    /**
     * @brief 工作线程的主循环函数
     *
     * 工作流程：
     * 1. 等待从队列中获取任务
     * 2. 如果获取成功，执行任务
     * 3. 重复直到线程池关闭
     *
     * @note 这是每个工作线程运行的函数
     * @note 在析构函数中通过join()等待其返回
     */
    void worker() {
        // 循环运行直到stop_标志被设置
        while (!stop_.load()) {
            Task task;

            // 从队列阻塞式获取任务
            // 如果线程池关闭，pop会返回false
            if (queue_.pop(task)) {
                try {
                    // 增加活跃任务计数
                    active_tasks_++;

                    // 执行任务
                    task();

                    // 减少活跃任务计数
                    active_tasks_--;
                } catch (const std::exception& e) {
                    // 捕获任务执行时的异常
                    // 防止异常导致线程退出
                    // 实际项目中应该记录日志
                    active_tasks_--;
                    // std::cerr << "Task exception: " << e.what() << std::endl;
                }
            }
        }
    }

private:
    std::vector<std::thread> threads_;          // 工作线程集合
    SafeQueue<Task> queue_;                     // 任务队列
    std::atomic<bool> stop_;                    // 关闭标志（原子操作）
    std::atomic<size_t> active_tasks_;          // 活跃任务计数（原子操作）
};

#endif // THREAD_POOL_H
