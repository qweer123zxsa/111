/*
 * SafeQueue.h - 线程安全的通用队列模板类
 *
 * 功能：
 * - 提供线程安全的FIFO队列
 * - 支持阻塞pop和非阻塞pop操作
 * - 使用std::queue和互斥锁实现
 *
 * 使用场景：
 * - 生产者-消费者模式
 * - 线程间数据传递
 * - 工作队列任务分发
 *
 * 作者：C++ 学习者
 * 日期：2025年
 */

#ifndef SAFE_QUEUE_H
#define SAFE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>

/**
 * @class SafeQueue
 * @brief 线程安全的队列，基于std::queue和互斥锁实现
 * @tparam T 队列中存储的数据类型
 *
 * 示例用法：
 * @code
 *   SafeQueue<int> queue;
 *
 *   // 生产者线程
 *   queue.push(42);
 *
 *   // 消费者线程 - 等待直到有元素或超时
 *   int value;
 *   if (queue.pop(value)) {
 *       std::cout << "Got value: " << value << std::endl;
 *   }
 *
 *   // 非阻塞pop
 *   std::optional<int> opt_value = queue.try_pop();
 *   if (opt_value) {
 *       std::cout << "Got value: " << *opt_value << std::endl;
 *   }
 * @endcode
 */
template <typename T>
class SafeQueue {
public:
    /**
     * @brief 默认构造函数
     * 初始化互斥锁和条件变量
     */
    SafeQueue() = default;

    /**
     * @brief 析构函数
     * 清空队列中的所有元素
     */
    ~SafeQueue() {
        clear();
    }

    /**
     * @brief 禁用拷贝构造函数
     * SafeQueue不支持拷贝，因为互斥锁不能被拷贝
     */
    SafeQueue(const SafeQueue&) = delete;

    /**
     * @brief 禁用拷贝赋值操作符
     */
    SafeQueue& operator=(const SafeQueue&) = delete;

    /**
     * @brief 支持移动构造函数
     * 允许用右值引用初始化
     */
    SafeQueue(SafeQueue&&) = default;

    /**
     * @brief 支持移动赋值操作符
     */
    SafeQueue& operator=(SafeQueue&&) = default;

    /**
     * @brief 向队列末尾添加元素
     * 线程安全，会通知等待的消费者
     *
     * @param value 要添加的元素引用（会被复制）
     *
     * @note 时间复杂度：O(1)
     * @note 会唤醒一个等待的消费者线程
     */
    void push(const T& value) {
        {
            // 使用std::lock_guard自动管理锁的生命周期
            // 构造时自动加锁，析构时自动解锁
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(value);
        }
        // 通知一个等待的消费者线程
        // 可以使用notify_one（唤醒一个）或notify_all（唤醒全部）
        condition_.notify_one();
    }

    /**
     * @brief 向队列末尾添加元素（使用右值引用）
     * 适用于大对象的高效传递
     *
     * @param value 要添加的元素右值引用
     *
     * @note 时间复杂度：O(1)
     */
    void push(T&& value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // std::move将左值转换为右值，避免不必要的拷贝
            queue_.push(std::move(value));
        }
        condition_.notify_one();
    }

    /**
     * @brief 阻塞式pop - 等待直到有元素可用
     *
     * @param[out] value 用于存储弹出的元素
     * @return true 如果成功弹出元素，false如果队列被关闭
     *
     * @note 如果队列为空，线程会阻塞直到有新元素
     * @note 时间复杂度：O(1)
     */
    bool pop(T& value) {
        // std::unique_lock提供了更灵活的锁管理
        // 支持lock(), unlock(), try_lock()等操作
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待直到队列不为空或shutdown_标志被设置
        // 这避免了虚假唤醒（spurious wakeup）
        condition_.wait(lock, [this] {
            return !queue_.empty() || shutdown_;
        });

        // 如果被关闭且队列为空，返回false
        if (shutdown_ && queue_.empty()) {
            return false;
        }

        // 如果队列不为空，弹出一个元素
        if (!queue_.empty()) {
            value = queue_.front();
            queue_.pop();
            return true;
        }

        return false;
    }

    /**
     * @brief 带超时的阻塞式pop
     *
     * @param[out] value 用于存储弹出的元素
     * @param timeout_ms 超时时间（毫秒）
     * @return true 如果成功弹出元素，false如果超时
     *
     * @note 支持毫秒级的超时控制
     */
    bool pop_for(T& value, int timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex_);

        // wait_for会在指定时间后返回，即使条件未满足
        // 返回值表示条件是否满足
        bool result = condition_.wait_for(lock,
            std::chrono::milliseconds(timeout_ms),
            [this] { return !queue_.empty() || shutdown_; });

        if (!result) {
            // 超时
            return false;
        }

        if (shutdown_ && queue_.empty()) {
            return false;
        }

        if (!queue_.empty()) {
            value = queue_.front();
            queue_.pop();
            return true;
        }

        return false;
    }

    /**
     * @brief 非阻塞式pop - 立即返回
     *
     * @param[out] value 用于存储弹出的元素
     * @return true 如果队列不为空且成功弹出，false如果队列为空
     *
     * @note 如果队列为空，立即返回false，不会阻塞
     * @note 时间复杂度：O(1)
     */
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty()) {
            return false;
        }

        value = queue_.front();
        queue_.pop();
        return true;
    }

    /**
     * @brief 返回队列中元素个数
     *
     * @return 当前队列中的元素个数
     *
     * @note 这个值可能在返回后立即改变（多线程环境）
     * @note 主要用于监控和调试，不要依赖其进行逻辑判断
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief 检查队列是否为空
     *
     * @return true 如果队列为空，false如果队列中有元素
     *
     * @note 同样受多线程影响，返回值可能立即失效
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief 清空队列中的所有元素
     * 线程安全操作
     *
     * @note 时间复杂度：O(n)，n为队列中的元素个数
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);

        // 弹出所有元素
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

    /**
     * @brief 关闭队列，唤醒所有等待的pop操作
     *
     * 使用场景：
     * - 线程池关闭时调用此函数
     * - 告知所有消费者线程停止等待
     * - 让所有pop操作立即返回false
     *
     * @note 关闭后队列中的现有元素仍可被弹出
     * @note 再次调用push会失败（可根据需要检查shutdown_标志）
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        // 唤醒所有等待的线程
        condition_.notify_all();
    }

    /**
     * @brief 检查队列是否已关闭
     *
     * @return true 如果队列已被shutdown()关闭
     */
    bool is_shutdown() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutdown_;
    }

private:
    std::queue<T> queue_;                          // 底层std::queue容器
    mutable std::mutex mutex_;                     // 保护queue_的互斥锁
    std::condition_variable condition_;            // 条件变量，用于线程间的信号传递
    bool shutdown_ = false;                        // 队列关闭标志
};

#endif // SAFE_QUEUE_H
