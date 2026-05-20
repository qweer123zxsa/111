/*
 * CircularBuffer.h - 循环缓冲区（Ring Buffer）实现
 *
 * 功能：
 * - 固定大小的循环缓冲区
 * - 高效的内存利用
 * - 线程安全操作
 *
 * 应用场景：
 * - 音视频帧缓冲
 * - 实时数据流处理
 * - 网络数据缓冲
 *
 * 原理：
 * 循环缓冲区使用两个指针（写指针和读指针）在固定大小的缓冲区中循环移动
 * 相比动态队列，避免了内存频繁分配释放
 * 特别适合实时系统和性能敏感的应用
 */

#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <cstring>
#include <mutex>
#include <memory>

/**
 * @class CircularBuffer
 * @brief 固定大小的循环缓冲区（字节级）
 *
 * 应用示例：
 * @code
 *   // 创建1MB的循环缓冲区
 *   CircularBuffer buffer(1024 * 1024);
 *
 *   // 写入数据
 *   const char* data = "Hello World";
 *   buffer.write(data, strlen(data));
 *
 *   // 读出数据
 *   char read_buf[12];
 *   int bytes_read = buffer.read(read_buf, sizeof(read_buf));
 * @endcode
 */
class CircularBuffer {
public:
    /**
     * @brief 构造函数 - 创建指定大小的循环缓冲区
     *
     * @param capacity 缓冲区容量（字节）
     *
     * @note capacity应该是2的幂次方以便于位运算优化
     * @note 会在堆上分配capacity大小的内存
     */
    explicit CircularBuffer(size_t capacity)
        : capacity_(capacity),
          buffer_(std::make_unique<uint8_t[]>(capacity)),
          write_pos_(0),
          read_pos_(0) {
    }

    /**
     * @brief 析构函数
     * buffer_会自动释放（unique_ptr特性）
     */
    ~CircularBuffer() = default;

    /**
     * @brief 禁用拷贝操作
     */
    CircularBuffer(const CircularBuffer&) = delete;
    CircularBuffer& operator=(const CircularBuffer&) = delete;

    /**
     * @brief 支持移动操作
     */
    CircularBuffer(CircularBuffer&&) noexcept = default;
    CircularBuffer& operator=(CircularBuffer&&) noexcept = default;

    /**
     * @brief 向缓冲区写入数据
     *
     * @param data 要写入的数据指针
     * @param size 要写入的数据大小（字节）
     * @return 实际写入的字节数
     *
     * @note 如果空闲空间不足，只会写入部分数据
     * @note 写入操作不会覆盖未读数据
     * @note 时间复杂度：O(1)
     */
    size_t write(const void* data, size_t size) {
        if (!data || size == 0) {
            return 0;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 计算可用的写入空间
        size_t available = capacity_ - available_data();

        // 如果请求写入超过可用空间，只写入可用部分
        size_t bytes_to_write = std::min(size, available);

        if (bytes_to_write == 0) {
            return 0; // 缓冲区满
        }

        const uint8_t* src = static_cast<const uint8_t*>(data);

        // 情况1：写入不会wrap around（绕过缓冲区末端）
        if (write_pos_ + bytes_to_write <= capacity_) {
            std::memcpy(buffer_.get() + write_pos_, src, bytes_to_write);
            write_pos_ = (write_pos_ + bytes_to_write) % capacity_;
        }
        // 情况2：写入会分两段
        else {
            // 第一段：从write_pos_到缓冲区末尾
            size_t first_part = capacity_ - write_pos_;
            std::memcpy(buffer_.get() + write_pos_, src, first_part);

            // 第二段：从缓冲区开始到某个位置
            size_t second_part = bytes_to_write - first_part;
            std::memcpy(buffer_.get(), src + first_part, second_part);

            write_pos_ = second_part;
        }

        return bytes_to_write;
    }

    /**
     * @brief 从缓冲区读取数据
     *
     * @param[out] data 用于存储读出数据的缓冲区
     * @param size 要读取的最大字节数
     * @return 实际读出的字节数
     *
     * @note 读取会移动读指针，数据不会重复读取
     * @note 时间复杂度：O(1)
     */
    size_t read(void* data, size_t size) {
        if (!data || size == 0) {
            return 0;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 计算可读的数据大小
        size_t bytes_available = available_data();
        size_t bytes_to_read = std::min(size, bytes_available);

        if (bytes_to_read == 0) {
            return 0; // 缓冲区为空
        }

        uint8_t* dest = static_cast<uint8_t*>(data);

        // 情况1：读取不会wrap around
        if (read_pos_ + bytes_to_read <= capacity_) {
            std::memcpy(dest, buffer_.get() + read_pos_, bytes_to_read);
            read_pos_ = (read_pos_ + bytes_to_read) % capacity_;
        }
        // 情况2：读取会分两段
        else {
            // 第一段：从read_pos_到缓冲区末尾
            size_t first_part = capacity_ - read_pos_;
            std::memcpy(dest, buffer_.get() + read_pos_, first_part);

            // 第二段：从缓冲区开始读取
            size_t second_part = bytes_to_read - first_part;
            std::memcpy(dest + first_part, buffer_.get(), second_part);

            read_pos_ = second_part;
        }

        return bytes_to_read;
    }

    /**
     * @brief 查看数据但不移动读指针（Peek操作）
     *
     * @param[out] data 用于存储查看的数据
     * @param size 最多查看的字节数
     * @return 实际可查看的字节数
     *
     * @note 不会改变读指针位置
     * @note 用于检查数据头部不想立即消费
     */
    size_t peek(void* data, size_t size) const {
        if (!data || size == 0) {
            return 0;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        size_t bytes_available = available_data();
        size_t bytes_to_peek = std::min(size, bytes_available);

        if (bytes_to_peek == 0) {
            return 0;
        }

        uint8_t* dest = static_cast<uint8_t*>(data);

        if (read_pos_ + bytes_to_peek <= capacity_) {
            std::memcpy(dest, buffer_.get() + read_pos_, bytes_to_peek);
        } else {
            size_t first_part = capacity_ - read_pos_;
            std::memcpy(dest, buffer_.get() + read_pos_, first_part);
            size_t second_part = bytes_to_peek - first_part;
            std::memcpy(dest + first_part, buffer_.get(), second_part);
        }

        return bytes_to_peek;
    }

    /**
     * @brief 获取缓冲区中已有的数据字节数
     *
     * @return 当前缓冲区中的数据字节数
     *
     * @note 线程安全
     */
    size_t available_data() const {
        // 这个方法需要加锁，所以不能在其他加锁的方法中调用
        std::lock_guard<std::mutex> lock(mutex_);

        if (write_pos_ >= read_pos_) {
            // 没有wrap around的情况
            return write_pos_ - read_pos_;
        } else {
            // 发生了wrap around
            return capacity_ - read_pos_ + write_pos_;
        }
    }

    /**
     * @brief 获取缓冲区剩余的可写空间
     *
     * @return 可写的字节数
     *
     * @note 线程安全
     */
    size_t available_space() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return capacity_ - available_data_unlocked();
    }

    /**
     * @brief 清空缓冲区
     * 重置读写指针到初始状态
     *
     * @note 线程安全
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        read_pos_ = 0;
        write_pos_ = 0;
    }

    /**
     * @brief 获取缓冲区总容量
     *
     * @return 缓冲区的总大小（字节）
     *
     * @note 这个值是不变的
     */
    size_t capacity() const {
        return capacity_;
    }

    /**
     * @brief 缓冲区是否为空
     *
     * @return true 如果没有数据可读
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return read_pos_ == write_pos_;
    }

    /**
     * @brief 缓冲区是否满
     *
     * @return true 如果没有可写空间
     */
    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_data_unlocked() == capacity_;
    }

private:
    /**
     * @brief 内部的available_data方法（未锁定版本）
     * 用于已经加过锁的方法中调用
     *
     * @return 当前缓冲区中的数据字节数
     *
     * @note 调用者必须确保已持有mutex_的锁
     */
    size_t available_data_unlocked() const {
        if (write_pos_ >= read_pos_) {
            return write_pos_ - read_pos_;
        } else {
            return capacity_ - read_pos_ + write_pos_;
        }
    }

private:
    const size_t capacity_;                        // 缓冲区总大小
    std::unique_ptr<uint8_t[]> buffer_;           // 缓冲区指针
    size_t write_pos_;                             // 下一个写入位置
    size_t read_pos_;                              // 下一个读出位置
    mutable std::mutex mutex_;                    // 保护读写指针的互斥锁
};

#endif // CIRCULAR_BUFFER_H
