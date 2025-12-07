/*
 * CaptureManager.h - 捕获管理器
 *
 * 功能：
 * - 统一管理视频和音频捕获
 * - 同步视频和音频时间戳
 * - 统计捕获性能指标
 * - 提供统一的控制接口
 *
 * 设计特点：
 * - 模块化设计，可独立管理视频和音频
 * - 灵活的配置和启动方式
 * - 完整的统计和监控
 * - 线程安全的资源管理
 *
 * 使用场景：
 * - 音视频同时捕获
 * - 媒体文件处理
 * - 流媒体采集
 * - 录制和直播
 */

#ifndef CAPTURE_MANAGER_H
#define CAPTURE_MANAGER_H

#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iostream>
#include <sstream>

#include "AVServer_11_VideoCapture.h"
#include "AVServer_12_AudioCapture.h"

// ============================================================================
// ======================== 捕获统计信息 =======================================
// ============================================================================

/**
 * @struct CaptureStatistics
 * @brief 捕获统计信息
 */
struct CaptureStatistics {
    // 视频统计
    uint64_t video_frames_captured;
    uint64_t video_frames_dropped;

    // 音频统计
    uint64_t audio_frames_captured;
    uint64_t audio_frames_dropped;

    // 时间信息
    std::chrono::steady_clock::time_point start_time;

    /**
     * @brief 构造函数 - 初始化为0
     */
    CaptureStatistics()
        : video_frames_captured(0),
          video_frames_dropped(0),
          audio_frames_captured(0),
          audio_frames_dropped(0),
          start_time(std::chrono::steady_clock::now()) {
    }

    /**
     * @brief 获取运行时间（秒）
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
     * @brief 获取统计信息字符串
     *
     * @return 格式化的统计信息
     */
    std::string to_string() const {
        std::ostringstream oss;
        oss << "=== Capture Statistics ===" << std::endl;
        oss << "Uptime: " << get_uptime_seconds() << "s" << std::endl;
        oss << "\nVideo:" << std::endl;
        oss << "  Frames Captured: " << video_frames_captured << std::endl;
        oss << "  Frames Dropped: " << video_frames_dropped << std::endl;
        oss << "\nAudio:" << std::endl;
        oss << "  Frames Captured: " << audio_frames_captured << std::endl;
        oss << "  Frames Dropped: " << audio_frames_dropped << std::endl;

        uint64_t total_captured = video_frames_captured + audio_frames_captured;
        uint64_t total_dropped = video_frames_dropped + audio_frames_dropped;
        oss << "\nTotal:" << std::endl;
        oss << "  Frames: " << total_captured << std::endl;
        oss << "  Dropped: " << total_dropped << std::endl;

        if (total_captured > 0) {
            double drop_rate = (total_dropped * 100.0) / total_captured;
            oss << "  Drop Rate: " << drop_rate << "%" << std::endl;
        }

        return oss.str();
    }
};

// ============================================================================
// ======================== 捕获管理器 =========================================
// ============================================================================

/**
 * @class CaptureManager
 * @brief 音视频捕获管理器
 *
 * 统一管理视频和音频的捕获过程，提供统一的启动、停止和数据获取接口。
 *
 * 使用示例：
 * @code
 *   // 创建管理器
 *   CaptureManager manager;
 *
 *   // 配置视频捕获
 *   VideoCaptureConfig video_config;
 *   video_config.width = 1280;
 *   video_config.height = 720;
 *   video_config.framerate = 30;
 *
 *   // 配置音频捕获
 *   AudioCaptureConfig audio_config;
 *   audio_config.sample_rate = 48000;
 *   audio_config.channels = 2;
 *
 *   // 设置配置
 *   manager.set_video_config(video_config);
 *   manager.set_audio_config(audio_config);
 *
 *   // 启动捕获
 *   if (!manager.start()) {
 *       std::cerr << "Failed to start capture" << std::endl;
 *       return 1;
 *   }
 *
 *   // 获取帧
 *   while (manager.is_running()) {
 *       auto video_frame = manager.get_video_frame();
 *       auto audio_frame = manager.get_audio_frame();
 *       // 处理帧...
 *   }
 *
 *   // 停止捕获
 *   manager.stop();
 * @endcode
 */
class CaptureManager {
public:
    /**
     * @brief 构造函数
     *
     * @param shared_pool 是否使用共享的帧缓冲池
     *
     * @note 如果shared_pool为true，视频和音频会共享一个缓冲池
     * @note 否则会分别创建各自的缓冲池
     */
    explicit CaptureManager(bool shared_pool = true)
        : shared_pool_(shared_pool),
          video_capture_(nullptr),
          audio_capture_(nullptr),
          frame_pool_(nullptr),
          running_(false),
          video_enabled_(false),
          audio_enabled_(false) {

        // 创建共享的帧缓冲池
        if (shared_pool) {
            frame_pool_ = std::make_shared<FrameBufferPool>(100);
        }

        std::cout << "[CaptureManager] Initialized" << std::endl;
    }

    /**
     * @brief 析构函数
     */
    ~CaptureManager() {
        stop();
    }

    /**
     * @brief 设置视频捕获配置
     *
     * @param config 视频捕获配置
     *
     * @note 该函数必须在start()之前调用
     */
    void set_video_config(const VideoCaptureConfig& config) {
        video_config_ = config;
        video_enabled_ = true;
        std::cout << "[CaptureManager] Video config set: " << config.width << "x"
                  << config.height << "@" << config.framerate << "fps" << std::endl;
    }

    /**
     * @brief 设置音频捕获配置
     *
     * @param config 音频捕获配置
     *
     * @note 该函数必须在start()之前调用
     */
    void set_audio_config(const AudioCaptureConfig& config) {
        audio_config_ = config;
        audio_enabled_ = true;
        std::cout << "[CaptureManager] Audio config set: " << config.sample_rate
                  << "Hz, " << config.channels << "ch" << std::endl;
    }

    /**
     * @brief 启动音视频捕获
     *
     * @return true 如果捕获启动成功
     *
     * 步骤：
     * 1. 验证至少有一种捕获方式被启用
     * 2. 创建视频捕获对象（如果启用）
     * 3. 创建音频捕获对象（如果启用）
     * 4. 启动捕获
     */
    bool start() {
        if (running_.load()) {
            return true;
        }

        // 检查是否至少启用了一种捕获
        if (!video_enabled_ && !audio_enabled_) {
            std::cerr << "[CaptureManager] No capture source enabled" << std::endl;
            return false;
        }

        std::cout << "[CaptureManager] Starting capture..." << std::endl;

        bool success = true;

        // 启动视频捕获
        if (video_enabled_) {
            auto pool = shared_pool_ ? frame_pool_ : std::make_shared<FrameBufferPool>(30);
            video_capture_ = std::make_shared<VideoCapture>(video_config_, pool);

            if (!video_capture_->start()) {
                std::cerr << "[CaptureManager] Failed to start video capture" << std::endl;
                success = false;
            }
        }

        // 启动音频捕获
        if (audio_enabled_) {
            auto pool = shared_pool_ ? frame_pool_ : std::make_shared<FrameBufferPool>(100);
            audio_capture_ = std::make_shared<AudioCapture>(audio_config_, pool);

            if (!audio_capture_->start()) {
                std::cerr << "[CaptureManager] Failed to start audio capture" << std::endl;
                success = false;
            }
        }

        if (success) {
            running_ = true;
            stats_.start_time = std::chrono::steady_clock::now();
            std::cout << "[CaptureManager] Capture started successfully" << std::endl;
        }

        return success;
    }

    /**
     * @brief 停止音视频捕获
     *
     * 步骤：
     * 1. 设置运行标志为false
     * 2. 停止视频捕获
     * 3. 停止音频捕获
     * 4. 输出最终统计
     */
    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {
            return;
        }

        std::cout << "[CaptureManager] Stopping capture..." << std::endl;

        // 停止视频捕获
        if (video_capture_) {
            video_capture_->stop();
            video_capture_ = nullptr;
        }

        // 停止音频捕获
        if (audio_capture_) {
            audio_capture_->stop();
            audio_capture_ = nullptr;
        }

        std::cout << "[CaptureManager] Capture stopped" << std::endl;
    }

    /**
     * @brief 检查捕获是否正在运行
     *
     * @return true 如果正在捕获
     */
    bool is_running() const {
        return running_.load();
    }

    // ===== 视频捕获接口 =====

    /**
     * @brief 获取视频帧
     *
     * @param timeout_ms 超时时间（毫秒）
     * @return 指向AVFrame的智能指针，如果超时返回nullptr
     */
    std::shared_ptr<AVFrame> get_video_frame(int timeout_ms = 1000) {
        if (!video_capture_) {
            return nullptr;
        }

        auto frame = video_capture_->get_frame(timeout_ms);
        if (frame) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.video_frames_captured++;
        }
        return frame;
    }

    /**
     * @brief 非阻塞式获取视频帧
     *
     * @return 指向AVFrame的智能指针，如果队列为空返回nullptr
     */
    std::shared_ptr<AVFrame> try_get_video_frame() {
        if (!video_capture_) {
            return nullptr;
        }
        return video_capture_->try_get_frame();
    }

    /**
     * @brief 获取视频捕获器
     *
     * @return 指向VideoCapture的指针
     */
    VideoCapture* get_video_capture() {
        return video_capture_.get();
    }

    // ===== 音频捕获接口 =====

    /**
     * @brief 获取音频帧
     *
     * @param timeout_ms 超时时间（毫秒）
     * @return 指向AVFrame的智能指针，如果超时返回nullptr
     */
    std::shared_ptr<AVFrame> get_audio_frame(int timeout_ms = 1000) {
        if (!audio_capture_) {
            return nullptr;
        }

        auto frame = audio_capture_->get_frame(timeout_ms);
        if (frame) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.audio_frames_captured++;
        }
        return frame;
    }

    /**
     * @brief 非阻塞式获取音频帧
     *
     * @return 指向AVFrame的智能指针，如果队列为空返回nullptr
     */
    std::shared_ptr<AVFrame> try_get_audio_frame() {
        if (!audio_capture_) {
            return nullptr;
        }
        return audio_capture_->try_get_frame();
    }

    /**
     * @brief 获取音频捕获器
     *
     * @return 指向AudioCapture的指针
     */
    AudioCapture* get_audio_capture() {
        return audio_capture_.get();
    }

    // ===== 统计和监控 =====

    /**
     * @brief 获取统计信息
     *
     * @return 捕获统计信息
     */
    CaptureStatistics get_statistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        CaptureStatistics stats = stats_;

        // 添加捕获器中的最新计数
        if (video_capture_) {
            stats.video_frames_dropped += video_capture_->get_dropped_frames();
        }
        if (audio_capture_) {
            stats.audio_frames_dropped += audio_capture_->get_dropped_frames();
        }

        return stats;
    }

    /**
     * @brief 输出统计信息
     */
    void print_statistics() const {
        std::cout << "\n" << get_statistics().to_string() << std::endl;
    }

    /**
     * @brief 获取视频队列大小
     *
     * @return 队列中的帧数
     */
    size_t get_video_queue_size() const {
        if (!video_capture_) {
            return 0;
        }
        return video_capture_->get_queue_size();
    }

    /**
     * @brief 获取音频队列大小
     *
     * @return 队列中的帧数
     */
    size_t get_audio_queue_size() const {
        if (!audio_capture_) {
            return 0;
        }
        return audio_capture_->get_queue_size();
    }

    /**
     * @brief 获取调试信息字符串
     *
     * @return 包含捕获状态的字符串
     */
    std::string get_debug_string() const {
        std::ostringstream oss;
        oss << "[CaptureManager] ";

        if (video_capture_) {
            oss << "Video(" << video_capture_->get_stats_string() << ") ";
        }

        if (audio_capture_) {
            oss << "Audio(" << audio_capture_->get_stats_string() << ")";
        }

        return oss.str();
    }

    /**
     * @brief 检查视频捕获是否启用
     *
     * @return true 如果启用了视频捕获
     */
    bool is_video_enabled() const {
        return video_enabled_;
    }

    /**
     * @brief 检查音频捕获是否启用
     *
     * @return true 如果启用了音频捕获
     */
    bool is_audio_enabled() const {
        return audio_enabled_;
    }

private:
    bool shared_pool_;                              // 是否使用共享缓冲池

    std::shared_ptr<VideoCapture> video_capture_;   // 视频捕获器
    std::shared_ptr<AudioCapture> audio_capture_;   // 音频捕获器
    std::shared_ptr<FrameBufferPool> frame_pool_;   // 共享帧缓冲池

    VideoCaptureConfig video_config_;               // 视频配置
    AudioCaptureConfig audio_config_;               // 音频配置

    std::atomic<bool> running_;                     // 运行状态
    bool video_enabled_;                            // 视频捕获是否启用
    bool audio_enabled_;                            // 音频捕获是否启用

    mutable std::mutex stats_mutex_;                // 保护统计信息的互斥锁
    CaptureStatistics stats_;                       // 捕获统计信息
};

#endif // CAPTURE_MANAGER_H
