/*
 * MediaProcessor.h - 媒体处理管道
 *
 * 功能：
 * - 整合捕获、编码、打包的完整处理流程
 * - 自动处理音视频帧
 * - 消息格式化和发送
 * - 性能监控和优化
 *
 * 处理流程：
 * Capture -> Encode -> Package -> Send to Network
 * 视频/音频帧 -> 压缩编码 -> 消息格式化 -> TCP发送
 *
 * 设计特点：
 * - 模块化的处理管道
 * - 自动的流量控制
 * - 实时的性能监控
 * - 线程安全的操作
 *
 * 使用场景：
 * - 实时视频直播
 * - 音视频流传输
 * - 自适应码率直播
 * - 多客户端转发
 */

#ifndef MEDIA_PROCESSOR_H
#define MEDIA_PROCESSOR_H

#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <iostream>
#include <queue>

#include "AVServer_13_CaptureManager.h"
#include "AVServer_14_CompressionEngine.h"
#include "AVServer_06_MessageProtocol.h"

// ============================================================================
// ======================== 媒体处理统计 =====================================
// ============================================================================

/**
 * @struct ProcessingStatistics
 * @brief 媒体处理的统计信息
 */
struct ProcessingStatistics {
    // 处理计数
    uint64_t total_video_frames;        // 处理的视频帧数
    uint64_t total_audio_frames;        // 处理的音频帧数
    uint64_t total_messages_sent;       // 发送的消息数

    // 数据量统计
    uint64_t total_video_bytes_sent;    // 发送的视频字节数
    uint64_t total_audio_bytes_sent;    // 发送的音频字节数

    // 性能指标
    double average_fps;                 // 平均帧率
    double average_latency_ms;          // 平均延迟（毫秒）

    // 队列统计
    size_t current_video_queue_size;    // 当前视频队列大小
    size_t current_audio_queue_size;    // 当前音频队列大小

    /**
     * @brief 构造函数
     */
    ProcessingStatistics()
        : total_video_frames(0),
          total_audio_frames(0),
          total_messages_sent(0),
          total_video_bytes_sent(0),
          total_audio_bytes_sent(0),
          average_fps(0.0),
          average_latency_ms(0.0),
          current_video_queue_size(0),
          current_audio_queue_size(0) {
    }

    /**
     * @brief 获取统计信息字符串
     *
     * @return 格式化的统计信息
     */
    std::string to_string() const {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer),
            "Processing Stats [Video: %llu frames/%.2fMB, Audio: %llu frames/%.2fMB, "
            "Messages: %llu, FPS: %.1f, Latency: %.2fms, "
            "Queues: V:%zu A:%zu]",
            total_video_frames, total_video_bytes_sent / (1024.0 * 1024.0),
            total_audio_frames, total_audio_bytes_sent / (1024.0 * 1024.0),
            total_messages_sent,
            average_fps, average_latency_ms,
            current_video_queue_size, current_audio_queue_size);
        return std::string(buffer);
    }
};

// ============================================================================
// ======================== 媒体处理器 ========================================
// ============================================================================

/**
 * @class MediaProcessor
 * @brief 音视频媒体处理器
 *
 * 工作流程：
 * 1. 从CaptureManager获取原始帧
 * 2. 通过CompressionEngine进行编码压缩
 * 3. 格式化为Message消息格式
 * 4. 放入发送队列
 * 5. 由外部发送到客户端
 *
 * 使用示例：
 * @code
 *   // 创建捕获和压缩管理器
 *   CaptureManager capture;
 *   CompressionConfig compress_config;
 *   CompressionEngine engine(compress_config);
 *
 *   // 创建处理器
 *   MediaProcessor processor(&capture, &engine);
 *
 *   // 启动处理
 *   processor.start();
 *
 *   // 获取处理后的消息
 *   while (processor.is_running()) {
 *       auto msg = processor.get_message();
 *       if (msg) {
 *           // 发送消息给客户端
 *           connection->send(*msg);
 *       }
 *   }
 *
 *   processor.stop();
 * @endcode
 */
class MediaProcessor {
public:
    /**
     * @brief 构造函数
     *
     * @param capture_mgr 捕获管理器指针
     * @param compress_engine 压缩引擎指针
     */
    MediaProcessor(CaptureManager* capture_mgr,
                  CompressionEngine* compress_engine)
        : capture_manager_(capture_mgr),
          compress_engine_(compress_engine),
          running_(false),
          process_thread_(),
          message_queue_(std::make_shared<SafeQueue<Message>>()),
          stats_(),
          stats_mutex_() {
        std::cout << "[MediaProcessor] Initialized" << std::endl;
    }

    /**
     * @brief 析构函数
     */
    ~MediaProcessor() {
        stop();
    }

    /**
     * @brief 启动媒体处理
     *
     * @return true 如果启动成功
     *
     * @note 启动处理线程
     * @note 捕获和压缩引擎应该已经启动
     */
    bool start() {
        if (running_.load()) {
            return true;
        }

        if (!capture_manager_ || !compress_engine_) {
            std::cerr << "[MediaProcessor] Missing capture or compression engine" << std::endl;
            return false;
        }

        if (!capture_manager_->is_running() || !compress_engine_->is_running()) {
            std::cerr << "[MediaProcessor] Capture or compression not running" << std::endl;
            return false;
        }

        running_ = true;
        process_thread_ = std::thread(&MediaProcessor::process_loop, this);

        std::cout << "[MediaProcessor] Started processing" << std::endl;
        return true;
    }

    /**
     * @brief 停止媒体处理
     *
     * @note 停止处理线程
     * @note 清空待处理队列
     */
    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {
            return;
        }

        // 等待处理线程完成
        if (process_thread_.joinable()) {
            process_thread_.join();
        }

        std::cout << "[MediaProcessor] Stopped" << std::endl;
    }

    /**
     * @brief 检查处理器是否运行
     *
     * @return true 如果处理器正在运行
     */
    bool is_running() const {
        return running_.load();
    }

    /**
     * @brief 从消息队列获取一条消息
     *
     * @param timeout_ms 超时时间（毫秒）
     * @return 消息对象，如果超时返回nullptr
     *
     * @note 该函数从发送队列获取已处理的消息
     */
    std::unique_ptr<Message> get_message(int timeout_ms = 1000) {
        Message msg;
        auto start = std::chrono::steady_clock::now();

        while (true) {
            if (message_queue_->pop(msg)) {
                return std::make_unique<Message>(msg);
            }

            // 检查超时
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) {
                return nullptr;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    /**
     * @brief 非阻塞式获取消息
     *
     * @return 消息对象，如果队列为空返回nullptr
     */
    std::unique_ptr<Message> try_get_message() {
        Message msg;
        if (message_queue_->try_pop(msg)) {
            return std::make_unique<Message>(msg);
        }
        return nullptr;
    }

    /**
     * @brief 获取队列中的消息数
     *
     * @return 待发送的消息数
     */
    size_t get_queue_size() const {
        return message_queue_->size();
    }

    /**
     * @brief 获取处理统计信息
     *
     * @return 处理统计结构体
     */
    ProcessingStatistics get_statistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        auto stats = stats_;

        // 更新队列大小
        if (capture_manager_) {
            stats.current_video_queue_size = capture_manager_->get_video_queue_size();
            stats.current_audio_queue_size = capture_manager_->get_audio_queue_size();
        }

        return stats;
    }

    /**
     * @brief 输出统计信息
     */
    void print_statistics() const {
        std::cout << get_statistics().to_string() << std::endl;
    }

    /**
     * @brief 设置目标比特率（自适应）
     *
     * @param bitrate 新的目标比特率（bps）
     */
    void set_target_bitrate(uint32_t bitrate) {
        if (compress_engine_) {
            compress_engine_->set_target_bitrate(bitrate);
        }
    }

    /**
     * @brief 获取消息队列大小（用于监控）
     *
     * @return 队列中的消息数
     */
    size_t get_pending_messages() const {
        return message_queue_->size();
    }

private:
    /**
     * @brief 处理线程主循环
     *
     * 工作流程：
     * 1. 获取视频帧，进行编码
     * 2. 获取音频帧，进行编码
     * 3. 格式化为消息
     * 4. 放入发送队列
     * 5. 更新统计信息
     */
    void process_loop() {
        auto frame_pool = std::make_shared<FrameBufferPool>(30);

        while (running_.load()) {
            bool has_frame = false;

            // 处理视频帧
            auto raw_video = capture_manager_->try_get_video_frame();
            if (raw_video) {
                has_frame = true;

                // 编码视频帧
                auto encoded_video = frame_pool->get();
                if (encoded_video && compress_engine_->encode_video(raw_video, encoded_video)) {
                    // 创建消息
                    Message msg(MessageType::VIDEO_FRAME, encoded_video->size,
                               ProtocolHelper::get_timestamp_ms());
                    msg.set_payload(encoded_video->data.data(), encoded_video->size);

                    // 放入发送队列
                    message_queue_->push(msg);

                    // 更新统计
                    {
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        stats_.total_video_frames++;
                        stats_.total_video_bytes_sent += encoded_video->size;
                        stats_.total_messages_sent++;
                    }

                    frame_pool->return_frame(encoded_video);
                }
                capture_manager_->get_video_capture()->get_frame_pool()->return_frame(raw_video);
            }

            // 处理音频帧
            auto raw_audio = capture_manager_->try_get_audio_frame();
            if (raw_audio) {
                has_frame = true;

                // 编码音频帧
                auto encoded_audio = frame_pool->get();
                if (encoded_audio && compress_engine_->encode_audio(raw_audio, encoded_audio)) {
                    // 创建消息
                    Message msg(MessageType::AUDIO_FRAME, encoded_audio->size,
                               ProtocolHelper::get_timestamp_ms());
                    msg.set_payload(encoded_audio->data.data(), encoded_audio->size);

                    // 放入发送队列
                    message_queue_->push(msg);

                    // 更新统计
                    {
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        stats_.total_audio_frames++;
                        stats_.total_audio_bytes_sent += encoded_audio->size;
                        stats_.total_messages_sent++;
                    }

                    frame_pool->return_frame(encoded_audio);
                }
                capture_manager_->get_audio_capture()->get_frame_pool()->return_frame(raw_audio);
            }

            // 如果没有帧，短暂睡眠以避免CPU浪费
            if (!has_frame) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

private:
    CaptureManager* capture_manager_;               // 捕获管理器指针
    CompressionEngine* compress_engine_;            // 压缩引擎指针

    std::atomic<bool> running_;                     // 运行状态
    std::thread process_thread_;                    // 处理线程

    std::shared_ptr<SafeQueue<Message>> message_queue_;  // 消息队列（待发送）

    mutable std::mutex stats_mutex_;                // 保护统计信息的互斥锁
    ProcessingStatistics stats_;                    // 处理统计信息
};

#endif // MEDIA_PROCESSOR_H
