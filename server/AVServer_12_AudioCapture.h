/*
 * AudioCapture.h - 音频捕获模块
 *
 * 功能：
 * - 从麦克风或音频文件捕获音频数据
 * - 支持多种采样率和声道数
 * - 自动音频编码和转码
 * - 适应性缓冲管理
 *
 * 设计特点：
 * - 模块化的捕获接口
 * - 缓冲区自动管理
 * - 线程安全的帧队列
 * - 灵活的配置参数
 *
 * 依赖：
 * - 实际实现需要集成PortAudio或ALSA等音频库
 * - 当前提供接口定义和模拟实现
 *
 * 使用场景：
 * - 实时麦克风捕获
 * - 音频文件播放
 * - 音频混音
 * - 语音处理
 */

#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <string>
#include <iostream>

#include "AVServer_03_FrameBuffer.h"
#include "AVServer_01_SafeQueue.h"

// ============================================================================
// ======================== 音频捕获配置 =======================================
// ============================================================================

/**
 * @struct AudioCaptureConfig
 * @brief 音频捕获配置参数
 *
 * 包含音频源、采样率、声道等参数
 */
struct AudioCaptureConfig {
    // 音频源类型
    enum class SourceType {
        MICROPHONE = 0,        // 麦克风
        FILE = 1,              // 音频文件
        LOOPBACK = 2,          // 系统音频回环（录音机功能）
        TEST_TONE = 3,         // 测试音调
    };

    SourceType source_type;         // 音频源类型
    std::string source_path;        // 源路径（设备ID或文件路径）

    uint32_t sample_rate;           // 采样率（Hz）：44100, 48000等
    uint32_t channels;              // 声道数：1=单声道, 2=立体声
    CodecType codec_type;           // 编码格式

    uint32_t bitrate;               // 目标比特率（bps）
    uint8_t quality;                // 质量级别（0-100）

    uint32_t buffer_size;           // 缓冲区大小（音频帧数）
    int timeout_ms;                 // 超时时间（毫秒）

    /**
     * @brief 构造函数 - 初始化为默认值
     */
    AudioCaptureConfig()
        : source_type(SourceType::MICROPHONE),
          source_path("0"),         // 默认使用麦克风0
          sample_rate(48000),       // 高质量音频
          channels(2),              // 立体声
          codec_type(CodecType::AAC),
          bitrate(128000),          // 128kbps
          quality(90),
          buffer_size(100),
          timeout_ms(5000) {
    }
};

// ============================================================================
// ======================== 音频捕获类 =========================================
// ============================================================================

/**
 * @class AudioCapture
 * @brief 音频捕获和处理类
 *
 * 工作流程：
 * 1. 初始化音频设备（麦克风或文件）
 * 2. 启动捕获线程
 * 3. 不断获取原始音频数据
 * 4. 放入缓冲队列供使用者获取
 * 5. 停止时优雅关闭
 *
 * 使用示例：
 * @code
 *   AudioCaptureConfig config;
 *   config.source_type = AudioCaptureConfig::SourceType::MICROPHONE;
 *   config.sample_rate = 48000;
 *   config.channels = 2;
 *
 *   AudioCapture capture(config);
 *
 *   if (!capture.start()) {
 *       std::cerr << "Failed to start capture" << std::endl;
 *       return;
 *   }
 *
 *   // 获取捕获的音频帧
 *   std::shared_ptr<AVFrame> frame = capture.get_frame();
 *   if (frame) {
 *       std::cout << "Got audio: " << frame->sample_rate << "Hz"
 *                 << " " << frame->channels << "ch" << std::endl;
 *   }
 *
 *   capture.stop();
 * @endcode
 *
 * @note 当前实现为模拟版本，实际使用需集成PortAudio或ALSA
 */
class AudioCapture {
public:
    /**
     * @brief 构造函数
     *
     * @param config 捕获配置参数
     * @param frame_pool 帧缓冲池（用于复用帧对象）
     */
    explicit AudioCapture(const AudioCaptureConfig& config,
                         std::shared_ptr<FrameBufferPool> frame_pool = nullptr)
        : config_(config),
          frame_pool_(frame_pool),
          capture_device_(nullptr),
          running_(false),
          frame_count_(0),
          dropped_frames_(0),
          capture_thread_() {

        // 如果没有提供帧池，创建一个
        if (!frame_pool_) {
            frame_pool_ = std::make_shared<FrameBufferPool>(config.buffer_size);
        }

        std::cout << "[AudioCapture] Initialized with " << config.sample_rate
                  << "Hz, " << config.channels << "ch" << std::endl;
    }

    /**
     * @brief 析构函数
     * 自动停止捕获
     */
    ~AudioCapture() {
        stop();
    }

    /**
     * @brief 启动音频捕获
     *
     * @return true 如果捕获启动成功
     *
     * @note 该函数会启动一个单独的线程进行捕获
     * @note 实际实现需要集成音频捕获库
     */
    bool start() {
        if (running_.load()) {
            return true;
        }

        // TODO: 在实际实现中，这里应该初始化捕获设备
        // 例如：
        // - 初始化PortAudio或ALSA
        // - 配置采样率、声道、格式
        // - 打开麦克风或音频文件

        std::cout << "[AudioCapture] Opening source: " << config_.source_path << std::endl;

        // 模拟：打开设备
        if (!open_device()) {
            std::cerr << "[AudioCapture] Failed to open device" << std::endl;
            return false;
        }

        running_ = true;
        capture_thread_ = std::thread(&AudioCapture::capture_loop, this);

        std::cout << "[AudioCapture] Started capturing" << std::endl;
        return true;
    }

    /**
     * @brief 停止音频捕获
     *
     * 步骤：
     * 1. 设置运行标志为false
     * 2. 等待捕获线程退出
     * 3. 关闭捕获设备
     * 4. 清空帧队列
     *
     * @note 可以多次调用，后续调用无效果
     */
    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {
            return;
        }

        // 等待捕获线程退出
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }

        // 关闭设备
        close_device();

        // 清空队列
        while (!frame_queue_.empty()) {
            std::shared_ptr<AVFrame> frame;
            frame_queue_.try_pop(frame);
        }

        std::cout << "[AudioCapture] Stopped" << std::endl;
    }

    /**
     * @brief 检查捕获是否正在运行
     *
     * @return true 如果捕获正在进行
     */
    bool is_running() const {
        return running_.load();
    }

    /**
     * @brief 获取一帧音频数据
     *
     * @param timeout_ms 超时时间（毫秒）
     * @return 指向AVFrame的智能指针，如果超时返回nullptr
     *
     * @note 该函数会阻塞等待直到有帧可用或超时
     * @note 返回的帧应该在使用完后立即处理或返回给池
     */
    std::shared_ptr<AVFrame> get_frame(int timeout_ms = 1000) {
        std::shared_ptr<AVFrame> frame;

        // 尝试从队列中获取帧
        auto start = std::chrono::steady_clock::now();
        while (running_.load()) {
            if (frame_queue_.try_pop(frame)) {
                return frame;
            }

            // 检查超时
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) {
                return nullptr;
            }

            // 短暂睡眠以避免busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return nullptr;
    }

    /**
     * @brief 非阻塞式获取一帧
     *
     * @return 指向AVFrame的智能指针，如果队列为空返回nullptr
     */
    std::shared_ptr<AVFrame> try_get_frame() {
        std::shared_ptr<AVFrame> frame;
        frame_queue_.try_pop(frame);
        return frame;
    }

    /**
     * @brief 获取配置信息
     *
     * @return 常量引用到配置
     */
    const AudioCaptureConfig& get_config() const {
        return config_;
    }

    /**
     * @brief 获取已捕获的总帧数
     *
     * @return 捕获的帧数
     */
    uint64_t get_frame_count() const {
        return frame_count_.load();
    }

    /**
     * @brief 获取丢弃的帧数
     *
     * @return 丢弃的帧数
     *
     * @note 当缓冲区满时会丢弃新帧
     */
    uint64_t get_dropped_frames() const {
        return dropped_frames_.load();
    }

    /**
     * @brief 获取队列中待处理的帧数
     *
     * @return 队列中的帧数
     */
    size_t get_queue_size() const {
        return frame_queue_.size();
    }

    /**
     * @brief 返回一个已使用的帧给帧池
     *
     * @param frame 要返回的帧
     *
     * @note 这支持帧的复用机制
     */
    void return_frame(std::shared_ptr<AVFrame> frame) {
        if (frame && frame_pool_) {
            frame_pool_->return_frame(frame);
        }
    }

    /**
     * @brief 获取统计信息字符串
     *
     * @return 包含捕获统计的字符串
     */
    std::string get_stats_string() const {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer),
                     "AudioCapture[frames=%llu, dropped=%llu, queue=%zu]",
                     frame_count_.load(), dropped_frames_.load(),
                     frame_queue_.size());
        return std::string(buffer);
    }

private:
    /**
     * @brief 打开捕获设备
     *
     * @return true 如果设备打开成功
     *
     * @note 根据source_type打开不同的设备
     */
    bool open_device() {
        switch (config_.source_type) {
            case AudioCaptureConfig::SourceType::MICROPHONE:
                return open_microphone();
            case AudioCaptureConfig::SourceType::FILE:
                return open_file();
            case AudioCaptureConfig::SourceType::TEST_TONE:
                return open_test_tone();
            default:
                return false;
        }
    }

    /**
     * @brief 打开麦克风
     *
     * @return true 如果麦克风打开成功
     *
     * @note 当前为模拟实现，实际需要调用PortAudio或ALSA API
     */
    bool open_microphone() {
        // TODO: 实际实现
        // Pa_OpenStream(&stream, &inputParameters, NULL, ...);

        std::cout << "[AudioCapture] Opening microphone: " << config_.source_path << std::endl;
        // 模拟：麦克风打开成功
        capture_device_ = reinterpret_cast<void*>(1);
        return true;
    }

    /**
     * @brief 打开音频文件
     *
     * @return true 如果文件打开成功
     */
    bool open_file() {
        // TODO: 实际实现
        // 使用FFmpeg或其他音频库读取文件

        std::cout << "[AudioCapture] Opening file: " << config_.source_path << std::endl;
        // 模拟：文件打开成功
        capture_device_ = reinterpret_cast<void*>(1);
        return true;
    }

    /**
     * @brief 打开测试音调（用于演示和测试）
     *
     * @return true 总是成功
     */
    bool open_test_tone() {
        std::cout << "[AudioCapture] Using test tone" << std::endl;
        capture_device_ = reinterpret_cast<void*>(1);
        return true;
    }

    /**
     * @brief 关闭捕获设备
     */
    void close_device() {
        if (capture_device_) {
            // TODO: 实际实现释放资源
            capture_device_ = nullptr;
        }
    }

    /**
     * @brief 捕获线程的主循环
     *
     * 工作流程：
     * 1. 从设备读取原始音频数据
     * 2. 创建或获取AVFrame对象
     * 3. 填充帧数据
     * 4. 放入队列供使用者获取
     * 5. 如果队列满，丢弃帧
     */
    void capture_loop() {
        while (running_.load()) {
            // 获取或创建帧对象
            auto frame = frame_pool_->get();
            if (!frame) {
                std::cerr << "[AudioCapture] Failed to get frame from pool" << std::endl;
                continue;
            }

            // 模拟捕获一帧
            if (!capture_frame(frame)) {
                frame_pool_->return_frame(frame);
                std::cerr << "[AudioCapture] Failed to capture frame" << std::endl;
                continue;
            }

            // 尝试放入队列
            // 如果队列满，尝试移除最早的帧
            while (frame_queue_.size() >= config_.buffer_size) {
                std::shared_ptr<AVFrame> old_frame;
                if (frame_queue_.try_pop(old_frame)) {
                    frame_pool_->return_frame(old_frame);
                    dropped_frames_++;
                } else {
                    break;
                }
            }

            // 放入队列
            frame_queue_.push(frame);

            // 控制捕获速率
            // 音频帧间隔 = (采样数 / 采样率) 秒
            // 对于1024采样点的帧：
            // 48000Hz: 1024/48000 = 21.3ms
            std::this_thread::sleep_for(
                std::chrono::milliseconds(20)  // 大约48KHz 960采样点
            );
        }
    }

    /**
     * @brief 捕获单一帧
     *
     * @param[out] frame 存储捕获的帧
     * @return true 如果捕获成功
     *
     * @note 这是一个模拟实现
     * @note 实际实现应该从音频源读取真实音频数据
     */
    bool capture_frame(std::shared_ptr<AVFrame> frame) {
        if (!frame || !capture_device_) {
            return false;
        }

        // TODO: 实际实现应该读取真实音频数据
        // 模拟实现：填充帧信息
        frame->frame_type = FrameType::AUDIO_FRAME;
        frame->codec_type = config_.codec_type;
        frame->sample_rate = config_.sample_rate;
        frame->channels = config_.channels;
        frame->bitrate = config_.bitrate;
        frame->quality = config_.quality;
        frame->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // 模拟音频帧数据
        // 一个音频帧通常包含固定数量的采样点
        // 例如：960或1024个采样点
        uint32_t samples_per_frame = 960;  // 960采样点/48KHz = 20ms
        uint32_t sample_size = 2;  // 16-bit
        uint32_t frame_size = samples_per_frame * sample_size * config_.channels;

        frame->data.resize(std::min(frame_size, 100000U));
        frame->size = frame->data.size();

        frame_count_++;
        return true;
    }

private:
    AudioCaptureConfig config_;                     // 捕获配置
    std::shared_ptr<FrameBufferPool> frame_pool_;   // 帧缓冲池
    void* capture_device_;                          // 捕获设备句柄

    std::atomic<bool> running_;                     // 运行状态
    std::atomic<uint64_t> frame_count_;             // 捕获的帧数
    std::atomic<uint64_t> dropped_frames_;          // 丢弃的帧数

    SafeQueue<std::shared_ptr<AVFrame>> frame_queue_;  // 帧队列
    std::thread capture_thread_;                    // 捕获线程
};

#endif // AUDIO_CAPTURE_H
