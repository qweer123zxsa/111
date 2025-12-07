/*
 * FrameBuffer.h - 音视频帧缓冲管理
 *
 * 功能：
 * - 定义音视频帧的数据结构
 * - 帧缓冲池管理
 * - 支持不同的编码格式
 *
 * 设计思想：
 * - 复用帧缓冲，减少内存分配
 * - 支持多种帧类型
 * - 线程安全的缓冲池
 */

#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include <cstdint>
#include <cstring>
#include <memory>
#include <queue>
#include <mutex>
#include <vector>

/**
 * @enum FrameType
 * @brief 音视频帧的类型定义
 */
enum class FrameType {
    VIDEO_I_FRAME = 0,              // 关键帧（I帧）
    VIDEO_P_FRAME = 1,              // 预测帧（P帧）
    VIDEO_B_FRAME = 2,              // 双向预测帧（B帧）
    AUDIO_FRAME = 3,                // 音频帧
};

/**
 * @enum CodecType
 * @brief 编解码器类型
 */
enum class CodecType {
    H264 = 0,                       // H.264视频编码
    H265 = 1,                       // H.265视频编码
    VP9 = 2,                        // VP9视频编码
    AAC = 3,                        // AAC音频编码
    MP3 = 4,                        // MP3音频编码
};

/**
 * @struct AVFrame
 * @brief 音视频帧数据结构
 *
 * 这是本系统中最重要的数据结构之一
 * 用于在系统各组件间传递音视频数据
 */
struct AVFrame {
    // ===== 帧基本信息 =====
    FrameType frame_type;                      // 帧类型
    CodecType codec_type;                      // 编码类型
    uint32_t width;                            // 视频宽度（像素）
    uint32_t height;                           // 视频高度（像素）
    uint32_t sample_rate;                      // 采样率（音频，Hz）
    uint32_t channels;                         // 通道数（音频）

    // ===== 时间戳 =====
    uint64_t timestamp;                        // 帧的时间戳（毫秒）
    uint64_t pts;                              // 显示时间戳（Presentation Time Stamp）

    // ===== 数据 =====
    std::vector<uint8_t> data;                 // 帧数据缓冲
    uint32_t size;                             // 数据大小（字节）

    // ===== 质量控制 =====
    uint32_t bitrate;                          // 比特率（bps）
    uint8_t quality;                           // 质量级别（0-100）

    /**
     * @brief 构造函数 - 初始化帧结构
     * @param type 帧类型
     * @param codec 编码类型
     * @param capacity 数据缓冲初始容量
     */
    AVFrame(FrameType type = FrameType::VIDEO_I_FRAME,
            CodecType codec = CodecType::H264,
            uint32_t capacity = 1024 * 1024)
        : frame_type(type),
          codec_type(codec),
          width(0),
          height(0),
          sample_rate(0),
          channels(0),
          timestamp(0),
          pts(0),
          size(0),
          bitrate(0),
          quality(80) {
        // 预分配缓冲以避免频繁内存分配
        data.reserve(capacity);
    }

    /**
     * @brief 复制构造函数
     */
    AVFrame(const AVFrame& other)
        : frame_type(other.frame_type),
          codec_type(other.codec_type),
          width(other.width),
          height(other.height),
          sample_rate(other.sample_rate),
          channels(other.channels),
          timestamp(other.timestamp),
          pts(other.pts),
          data(other.data),
          size(other.size),
          bitrate(other.bitrate),
          quality(other.quality) {
    }

    /**
     * @brief 赋值操作符
     */
    AVFrame& operator=(const AVFrame& other) {
        if (this != &other) {
            frame_type = other.frame_type;
            codec_type = other.codec_type;
            width = other.width;
            height = other.height;
            sample_rate = other.sample_rate;
            channels = other.channels;
            timestamp = other.timestamp;
            pts = other.pts;
            data = other.data;
            size = other.size;
            bitrate = other.bitrate;
            quality = other.quality;
        }
        return *this;
    }

    /**
     * @brief 清空帧数据
     * 重置为初始状态但保留预分配的缓冲
     */
    void clear() {
        data.clear();
        size = 0;
        timestamp = 0;
        pts = 0;
    }

    /**
     * @brief 获取帧类型的字符串描述
     *
     * @return 帧类型的文字描述
     */
    const char* frame_type_str() const {
        switch (frame_type) {
            case FrameType::VIDEO_I_FRAME: return "I-Frame";
            case FrameType::VIDEO_P_FRAME: return "P-Frame";
            case FrameType::VIDEO_B_FRAME: return "B-Frame";
            case FrameType::AUDIO_FRAME: return "Audio-Frame";
            default: return "Unknown";
        }
    }

    /**
     * @brief 获取编码类型的字符串描述
     *
     * @return 编码类型的文字描述
     */
    const char* codec_type_str() const {
        switch (codec_type) {
            case CodecType::H264: return "H.264";
            case CodecType::H265: return "H.265";
            case CodecType::VP9: return "VP9";
            case CodecType::AAC: return "AAC";
            case CodecType::MP3: return "MP3";
            default: return "Unknown";
        }
    }
};

/**
 * @class FrameBufferPool
 * @brief 帧缓冲池 - 用于复用AVFrame对象
 *
 * 设计目的：
 * - 避免频繁的内存分配和释放
 * - 提高实时性能
 * - 降低GC压力
 *
 * 使用场景：
 * - 高帧率视频处理
 * - 音频流处理
 * - 实时性敏感的应用
 *
 * 示例用法：
 * @code
 *   FrameBufferPool pool(10);  // 预创建10个帧缓冲
 *
 *   auto frame = pool.get();
 *   // 使用frame...
 *
 *   pool.return_frame(frame);  // 归还给池
 * @endcode
 */
class FrameBufferPool {
public:
    /**
     * @brief 构造函数 - 创建帧缓冲池
     *
     * @param pool_size 池中预创建的帧数量
     * @param frame_capacity 每个帧的数据缓冲初始大小
     *
     * @note pool_size应该根据实时性要求调整
     * @note 典型值：30fps视频需要pool_size >= 3-5
     */
    FrameBufferPool(size_t pool_size = 10, uint32_t frame_capacity = 1024 * 1024)
        : pool_size_(pool_size),
          frame_capacity_(frame_capacity),
          stats_total_get_(0),
          stats_total_return_(0) {
        // 预创建pool_size个AVFrame对象
        for (size_t i = 0; i < pool_size; ++i) {
            available_frames_.push(
                std::make_shared<AVFrame>(
                    FrameType::VIDEO_I_FRAME,
                    CodecType::H264,
                    frame_capacity
                )
            );
        }
    }

    /**
     * @brief 析构函数
     */
    ~FrameBufferPool() {
        clear();
    }

    /**
     * @brief 从池中获取一个帧缓冲
     *
     * @return 指向AVFrame的智能指针
     *
     * @note 如果池为空，会创建一个新的帧对象
     * @note 获取的帧应该在使用完后调用return_frame归还
     */
    std::shared_ptr<AVFrame> get() {
        std::lock_guard<std::mutex> lock(mutex_);

        std::shared_ptr<AVFrame> frame;

        if (!available_frames_.empty()) {
            // 从池中获取一个可用的帧
            frame = available_frames_.front();
            available_frames_.pop();
        } else {
            // 池为空，创建新帧
            frame = std::make_shared<AVFrame>(
                FrameType::VIDEO_I_FRAME,
                CodecType::H264,
                frame_capacity_
            );
        }

        // 清空数据但保留缓冲
        frame->clear();

        // 统计信息
        stats_total_get_++;

        return frame;
    }

    /**
     * @brief 将帧缓冲归还给池
     *
     * @param frame 要归还的帧指针
     *
     * @note 归还的帧会被清空但保留容量
     * @note 调用者不应该再使用归还的帧
     */
    void return_frame(std::shared_ptr<AVFrame> frame) {
        if (!frame) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 清空数据
        frame->clear();

        // 如果池还没满，归还给池
        if (available_frames_.size() < pool_size_) {
            available_frames_.push(frame);
        }
        // 否则让智能指针自动销毁

        // 统计信息
        stats_total_return_++;
    }

    /**
     * @brief 获取池中当前可用的帧数
     *
     * @return 可用帧的数量
     */
    size_t available_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_frames_.size();
    }

    /**
     * @brief 清空池中的所有帧
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!available_frames_.empty()) {
            available_frames_.pop();
        }
    }

    /**
     * @brief 获取池的统计信息
     *
     * @return 一对数值（总get次数，总return次数）
     */
    std::pair<uint64_t, uint64_t> get_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {stats_total_get_, stats_total_return_};
    }

private:
    size_t pool_size_;                          // 池的目标大小
    uint32_t frame_capacity_;                   // 每个帧的缓冲初始大小
    std::queue<std::shared_ptr<AVFrame>> available_frames_;  // 可用帧队列
    mutable std::mutex mutex_;                  // 保护队列的互斥锁

    // ===== 统计信息 =====
    uint64_t stats_total_get_;                  // 总获取次数
    uint64_t stats_total_return_;               // 总归还次数
};

#endif // FRAME_BUFFER_H
