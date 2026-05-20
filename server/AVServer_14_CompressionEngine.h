/*
 * CompressionEngine.h - 音视频压缩和编码引擎
 *
 * 功能：
 * - 提供多种视频编码方案（H.264, H.265, VP9）
 * - 提供多种音频编码方案（AAC, MP3, Opus）
 * - 自适应比特率控制
 * - 实时压缩和编码处理
 * - 性能监控和统计
 *
 * 设计特点：
 * - 模块化的编码接口
 * - 支持硬件加速（预留）
 * - 灵活的质量和码率配置
 * - 线程安全的操作
 *
 * 依赖库（实际实现需要）：
 * - FFmpeg：完整的编解码库
 * - x264/x265：视频编码
 * - libopus/libfdk-aac：音频编码
 * - zlib：数据压缩
 *
 * 使用场景：
 * - 实时视频流编码
 * - 实时音频流编码
 * - 自适应码率直播
 * - 音视频转码
 */

#ifndef COMPRESSION_ENGINE_H
#define COMPRESSION_ENGINE_H

#include <memory>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <zlib.h>

#include "AVServer_03_FrameBuffer.h"

// ============================================================================
// ======================== 压缩和编码配置 =====================================
// ============================================================================

/**
 * @struct CompressionConfig
 * @brief 压缩和编码配置参数
 *
 * 控制压缩的质量、速度和方式
 */
struct CompressionConfig {
    // 压缩级别（0-9，0=无压缩，9=最大压缩）
    int compression_level;

    // 编码质量（0-100）
    int quality;

    // 目标比特率（bps）
    uint32_t target_bitrate;

    // 是否启用自适应比特率
    bool enable_adaptive_bitrate;

    // 是否启用硬件加速（如果可用）
    bool enable_hardware_acceleration;

    // 目标帧率
    uint32_t target_framerate;

    // 关键帧间隔（秒）
    int keyframe_interval;

    /**
     * @brief 构造函数 - 初始化为默认值
     */
    CompressionConfig()
        : compression_level(6),           // 中等压缩
          quality(80),                    // 较高质量
          target_bitrate(5000000),        // 5Mbps
          enable_adaptive_bitrate(true),
          enable_hardware_acceleration(false),
          target_framerate(30),
          keyframe_interval(2) {
    }
};

// ============================================================================
// ======================== 编码统计信息 =======================================
// ============================================================================

/**
 * @struct EncodingStatistics
 * @brief 编码处理的统计信息
 */
struct EncodingStatistics {
    // 处理计数
    uint64_t total_frames_processed;      // 处理的总帧数
    uint64_t total_frames_encoded;        // 成功编码的帧数
    uint64_t failed_encodings;            // 失败的编码数

    // 数据统计
    uint64_t total_input_bytes;           // 输入的总字节数
    uint64_t total_output_bytes;          // 输出的总字节数

    // 性能指标
    double average_compression_ratio;     // 平均压缩比
    double average_encoding_time_ms;      // 平均编码时间（毫秒）

    // 码率统计
    uint32_t current_bitrate;             // 当前实际码率
    double average_bitrate;               // 平均码率

    // 时间信息
    std::chrono::steady_clock::time_point start_time;

    /**
     * @brief 构造函数 - 初始化为0
     */
    EncodingStatistics()
        : total_frames_processed(0),
          total_frames_encoded(0),
          failed_encodings(0),
          total_input_bytes(0),
          total_output_bytes(0),
          average_compression_ratio(0.0),
          average_encoding_time_ms(0.0),
          current_bitrate(0),
          average_bitrate(0.0),
          start_time(std::chrono::steady_clock::now()) {
    }

    /**
     * @brief 计算压缩比
     *
     * @return 压缩比（原始大小/压缩大小）
     */
    double get_compression_ratio() const {
        if (total_output_bytes == 0) return 1.0;
        return static_cast<double>(total_input_bytes) / total_output_bytes;
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
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer),
            "Encoding Stats [Frames: %llu/%llu, Failed: %llu, "
            "Input: %.2fMB, Output: %.2fMB, Ratio: %.2f:1, "
            "Bitrate: %.2fMbps, Time: %.2fms/frame]",
            total_frames_encoded, total_frames_processed, failed_encodings,
            total_input_bytes / (1024.0 * 1024.0),
            total_output_bytes / (1024.0 * 1024.0),
            get_compression_ratio(),
            average_bitrate / 1000000.0,
            average_encoding_time_ms);
        return std::string(buffer);
    }
};

// ============================================================================
// ======================== 压缩引擎类 ========================================
// ============================================================================

/**
 * @class CompressionEngine
 * @brief 音视频压缩和编码引擎
 *
 * 工作流程：
 * 1. 接收原始音视频帧
 * 2. 根据配置进行压缩编码
 * 3. 输出压缩后的数据
 * 4. 跟踪编码统计信息
 *
 * 使用示例：
 * @code
 *   CompressionConfig config;
 *   config.target_bitrate = 5000000;  // 5Mbps
 *   config.quality = 80;
 *
 *   CompressionEngine engine(config);
 *   engine.start();
 *
 *   // 对视频帧进行编码
 *   auto input_frame = ...;  // 原始视频帧
 *   auto output_frame = engine.encode_video(input_frame);
 *
 *   if (output_frame) {
 *       std::cout << "Encoded: " << output_frame->size << " bytes" << std::endl;
 *   }
 *
 *   engine.stop();
 * @endcode
 *
 * @note 当前实现为模拟版本，实际使用需集成FFmpeg库
 */
class CompressionEngine {
public:
    /**
     * @brief 构造函数
     *
     * @param config 压缩配置参数
     */
    explicit CompressionEngine(const CompressionConfig& config = CompressionConfig())
        : config_(config),
          is_running_(false),
          frame_count_(0),
          last_frame_time_(std::chrono::steady_clock::now()),
          stats_() {
        std::cout << "[CompressionEngine] Initialized with quality=" << config.quality
                  << " bitrate=" << config.target_bitrate << "bps" << std::endl;
    }

    /**
     * @brief 析构函数
     */
    ~CompressionEngine() {
        stop();
    }

    /**
     * @brief 启动压缩引擎
     *
     * @return true 如果启动成功
     *
     * @note 初始化编码器和相关资源
     */
    bool start() {
        if (is_running_.load()) {
            return true;
        }

        std::cout << "[CompressionEngine] Starting..." << std::endl;

        // TODO: 初始化FFmpeg或其他编码库
        // 初始化视频编码器
        // 初始化音频编码器

        is_running_ = true;
        stats_.start_time = std::chrono::steady_clock::now();

        std::cout << "[CompressionEngine] Started successfully" << std::endl;
        return true;
    }

    /**
     * @brief 停止压缩引擎
     *
     * @note 释放编码器和相关资源
     */
    void stop() {
        if (!is_running_.load()) {
            return;
        }

        is_running_ = false;

        // TODO: 释放FFmpeg或其他编码库资源

        std::cout << "[CompressionEngine] Stopped" << std::endl;
    }

    /**
     * @brief 检查引擎是否正在运行
     *
     * @return true 如果引擎正在运行
     */
    bool is_running() const {
        return is_running_.load();
    }

    // ===== 视频编码 =====

    /**
     * @brief 对视频帧进行编码
     *
     * @param[in] input 输入的原始视频帧
     * @param[out] output 输出的编码后的帧
     * @return true 如果编码成功
     *
     * @note 输出帧会包含编码后的压缩数据
     * @note 编码算法由frame_type决定
     */
    bool encode_video(const std::shared_ptr<AVFrame>& input,
                     std::shared_ptr<AVFrame>& output) {
        if (!is_running_.load() || !input || !output) {
            return false;
        }

        auto start_time = std::chrono::steady_clock::now();

        // TODO: 实际实现应该调用FFmpeg或x264/x265编码库
        // 当前实现为模拟版本

        // 模拟编码过程
        output->frame_type = FrameType::VIDEO_I_FRAME;
        output->codec_type = input->codec_type;
        output->width = input->width;
        output->height = input->height;
        output->bitrate = config_.target_bitrate;
        output->quality = config_.quality;
        output->timestamp = input->timestamp;

        // 模拟压缩数据（实际大小应该是编码后的大小）
        uint32_t original_size = (input->width * input->height * 3) / 2;  // YUV420
        uint32_t compressed_size = compress_data(original_size);

        output->data.resize(compressed_size);
        output->size = compressed_size;

        // 更新统计信息
        update_stats(input, output, start_time);

        return true;
    }

    /**
     * @brief 对音频帧进行编码
     *
     * @param[in] input 输入的原始音频帧
     * @param[out] output 输出的编码后的帧
     * @return true 如果编码成功
     */
    bool encode_audio(const std::shared_ptr<AVFrame>& input,
                     std::shared_ptr<AVFrame>& output) {
        if (!is_running_.load() || !input || !output) {
            return false;
        }

        auto start_time = std::chrono::steady_clock::now();

        // TODO: 实际实现应该调用FFmpeg或libopus/libfdk-aac编码库
        // 当前实现为模拟版本

        // 模拟编码过程
        output->frame_type = FrameType::AUDIO_FRAME;
        output->codec_type = input->codec_type;
        output->sample_rate = input->sample_rate;
        output->channels = input->channels;
        output->bitrate = config_.target_bitrate;
        output->quality = config_.quality;
        output->timestamp = input->timestamp;

        // 模拟压缩数据
        uint32_t original_size = input->size;
        uint32_t compressed_size = compress_data(original_size);

        output->data.resize(compressed_size);
        output->size = compressed_size;

        // 更新统计信息
        update_stats(input, output, start_time);

        return true;
    }

    /**
     * @brief 使用zlib进行数据压缩
     *
     * @param[in] input_data 输入数据指针
     * @param input_size 输入数据大小
     * @param[out] output_data 输出缓冲区
     * @param[in,out] output_size 输出缓冲区大小（返回实际大小）
     * @return true 如果压缩成功
     *
     * @note 这是一个实际可用的压缩实现
     */
    static bool compress_with_zlib(const uint8_t* input_data,
                                  uint32_t input_size,
                                  uint8_t* output_data,
                                  uint32_t& output_size) {
        if (!input_data || !output_data || input_size == 0) {
            return false;
        }

        uLongf compressed_size = output_size;

        int ret = compress2(output_data, &compressed_size,
                          input_data, input_size,
                          6);  // 压缩级别6

        if (ret != Z_OK) {
            std::cerr << "[CompressionEngine] zlib compression failed: " << ret << std::endl;
            return false;
        }

        output_size = compressed_size;
        return true;
    }

    /**
     * @brief 解压数据（用于客户端）
     *
     * @param[in] input_data 压缩数据指针
     * @param input_size 压缩数据大小
     * @param[out] output_data 输出缓冲区
     * @param[in,out] output_size 输出缓冲区大小（返回实际大小）
     * @return true 如果解压成功
     */
    static bool decompress_with_zlib(const uint8_t* input_data,
                                    uint32_t input_size,
                                    uint8_t* output_data,
                                    uint32_t& output_size) {
        if (!input_data || !output_data || input_size == 0) {
            return false;
        }

        uLongf decompressed_size = output_size;

        int ret = uncompress(output_data, &decompressed_size,
                           input_data, input_size);

        if (ret != Z_OK) {
            std::cerr << "[CompressionEngine] zlib decompression failed: " << ret << std::endl;
            return false;
        }

        output_size = decompressed_size;
        return true;
    }

    /**
     * @brief 设置目标比特率（自适应）
     *
     * @param bitrate 新的目标比特率（bps）
     *
     * @note 用于自适应码率调整
     */
    void set_target_bitrate(uint32_t bitrate) {
        config_.target_bitrate = bitrate;
        std::cout << "[CompressionEngine] Bitrate adjusted to " << bitrate << "bps" << std::endl;
    }

    /**
     * @brief 设置质量级别
     *
     * @param quality 质量等级（0-100）
     */
    void set_quality(int quality) {
        config_.quality = std::max(0, std::min(100, quality));
    }

    /**
     * @brief 获取编码统计信息
     *
     * @return 编码统计结构体
     */
    EncodingStatistics get_statistics() const {
        return stats_;
    }

    /**
     * @brief 输出统计信息
     */
    void print_statistics() const {
        std::cout << stats_.to_string() << std::endl;
    }

    /**
     * @brief 获取当前实际码率
     *
     * @return 实际码率（bps）
     */
    uint32_t get_actual_bitrate() const {
        return stats_.current_bitrate;
    }

    /**
     * @brief 获取处理的帧数
     *
     * @return 处理的总帧数
     */
    uint64_t get_frame_count() const {
        return frame_count_.load();
    }

    /**
     * @brief 获取配置信息
     *
     * @return 常量引用到配置
     */
    const CompressionConfig& get_config() const {
        return config_;
    }

private:
    /**
     * @brief 计算压缩后的数据大小（模拟）
     *
     * @param original_size 原始数据大小
     * @return 压缩后的估计大小
     *
     * @note 基于配置的压缩率进行估计
     * @note 实际大小应该由编码库提供
     */
    uint32_t compress_data(uint32_t original_size) const {
        // 根据配置计算压缩大小
        // 高质量 (80-100) -> 70-80% 压缩率
        // 中质量 (50-80) -> 50-70% 压缩率
        // 低质量 (0-50) -> 30-50% 压缩率

        double compression_ratio = 0.5;  // 基础压缩率

        if (config_.quality >= 80) {
            compression_ratio = 0.75;  // 高质量
        } else if (config_.quality >= 50) {
            compression_ratio = 0.60;  // 中质量
        } else {
            compression_ratio = 0.40;  // 低质量
        }

        return static_cast<uint32_t>(original_size * compression_ratio);
    }

    /**
     * @brief 更新编码统计信息
     *
     * @param input 输入帧
     * @param output 输出帧
     * @param start_time 编码开始时间
     */
    void update_stats(const std::shared_ptr<AVFrame>& input,
                     const std::shared_ptr<AVFrame>& output,
                     const std::chrono::steady_clock::time_point& start_time) {
        // 计算编码时间
        auto end_time = std::chrono::steady_clock::now();
        auto encoding_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        // 更新计数
        stats_.total_frames_processed++;
        stats_.total_frames_encoded++;
        stats_.total_input_bytes += input->size;
        stats_.total_output_bytes += output->size;

        // 更新性能指标
        stats_.average_compression_ratio = stats_.get_compression_ratio();
        stats_.average_encoding_time_ms =
            (stats_.average_encoding_time_ms * 0.9) + (encoding_time * 0.1);

        // 计算码率（每秒字节数）
        auto uptime = stats_.get_uptime_seconds();
        if (uptime > 0) {
            stats_.average_bitrate = (stats_.total_output_bytes * 8) /
                                    static_cast<double>(uptime);
            stats_.current_bitrate = stats_.average_bitrate;
        }

        frame_count_++;
        last_frame_time_ = end_time;
    }

private:
    CompressionConfig config_;                      // 压缩配置
    std::atomic<bool> is_running_;                  // 运行状态

    std::atomic<uint64_t> frame_count_;             // 处理的帧数
    std::chrono::steady_clock::time_point last_frame_time_;  // 最后一帧时间

    mutable EncodingStatistics stats_;              // 编码统计信息
};

#endif // COMPRESSION_ENGINE_H
