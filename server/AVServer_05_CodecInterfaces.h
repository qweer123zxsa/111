/*
 * VideoCodec.h 和 AudioCodec.h - 音视频编解码接口
 *
 * 功能：
 * - 定义编解码器的标准接口
 * - 支持多种编码格式
 * - 可扩展的编解码器注册系统
 *
 * 设计模式：
 * 使用抽象基类定义接口，具体编解码器继承该接口
 * 这样可以轻松添加新的编码格式支持
 *
 * 真实实现需要依赖FFmpeg库
 * 本示例代码提供接口定义和模拟实现
 */

#ifndef VIDEO_CODEC_H
#define VIDEO_CODEC_H

#include <memory>
#include <vector>
#include <cstdint>
#include "FrameBuffer.h"

// ============================================================================
// ======================== VideoCodec（视频编解码器）=======================
// ============================================================================

/**
 * @class VideoCodec
 * @brief 视频编解码器抽象基类
 *
 * 定义视频编码和解码的标准接口
 * 所有具体的编解码器应该继承此类并实现其方法
 */
class VideoCodec {
public:
    /**
     * @brief 虚拟析构函数
     * 确保继承类的析构函数能被正确调用
     */
    virtual ~VideoCodec() = default;

    /**
     * @brief 初始化编码器
     *
     * @param width 输入视频的宽度（像素）
     * @param height 输入视频的高度（像素）
     * @param bitrate 目标比特率（bps）
     * @param framerate 帧率（fps）
     * @return true 如果初始化成功
     *
     * @note 必须在encode之前调用
     * @note 不同的编解码器可能有不同的参数限制
     */
    virtual bool init_encoder(uint32_t width, uint32_t height,
                            uint32_t bitrate, uint32_t framerate) = 0;

    /**
     * @brief 初始化解码器
     *
     * @return true 如果初始化成功
     *
     * @note 解码器通常不需要提前初始化宽高
     * @note 从码流中自动检测参数
     */
    virtual bool init_decoder() = 0;

    /**
     * @brief 编码原始视频帧
     *
     * 过程：
     * 1. 接收原始视频数据（YUV420或其他格式）
     * 2. 使用编码器压缩
     * 3. 返回压缩后的比特流
     *
     * @param[in] input 输入的原始视频数据
     * @param[out] output 输出的编码后的数据
     * @return 编码是否成功
     *
     * @note input中的data字段应该包含原始YUV数据
     * @note output->data将包含编码后的H.264等码流
     * @note output->size为编码后数据的大小
     */
    virtual bool encode(const AVFrame& input, AVFrame& output) = 0;

    /**
     * @brief 解码比特流到原始帧
     *
     * 过程：
     * 1. 接收编码的比特流（如H.264）
     * 2. 使用解码器解码
     * 3. 返回原始视频数据
     *
     * @param[in] input 输入的编码码流
     * @param[out] output 输出的原始视频数据
     * @return 解码是否成功
     *
     * @note input->data应包含编码的码流
     * @note output->data将包含解码后的YUV数据
     */
    virtual bool decode(const AVFrame& input, AVFrame& output) = 0;

    /**
     * @brief 获取编码器的编码类型
     *
     * @return 编码格式（H264、H265等）
     */
    virtual CodecType get_codec_type() const = 0;

    /**
     * @brief 获取编码器的当前码率（bps）
     *
     * @return 实际输出的比特率
     */
    virtual uint32_t get_bitrate() const = 0;

    /**
     * @brief 设置编码器的码率
     *
     * @param bitrate 新的目标比特率（bps）
     * @return 设置是否成功
     *
     * @note 可以在编码过程中动态调整
     * @note 用于自适应码率调整
     */
    virtual bool set_bitrate(uint32_t bitrate) = 0;

    /**
     * @brief 清空编码器的缓冲
     * 用于处理编码器内部的缓冲数据
     */
    virtual void flush() = 0;

    /**
     * @brief 关闭编码/解码器
     * 释放所有资源
     */
    virtual void close() = 0;
};

// ============================================================================
// ======================== AudioCodec（音频编解码器）=======================
// ============================================================================

/**
 * @class AudioCodec
 * @brief 音频编解码器抽象基类
 *
 * 定义音频编码和解码的标准接口
 */
class AudioCodec {
public:
    /**
     * @brief 虚拟析构函数
     */
    virtual ~AudioCodec() = default;

    /**
     * @brief 初始化音频编码器
     *
     * @param sample_rate 采样率（Hz），通常44100或48000
     * @param channels 通道数（1=mono, 2=stereo）
     * @param bitrate 目标比特率（bps）
     * @return true 如果初始化成功
     *
     * @note 音频参数必须与输入数据匹配
     */
    virtual bool init_encoder(uint32_t sample_rate, uint32_t channels,
                            uint32_t bitrate) = 0;

    /**
     * @brief 初始化音频解码器
     *
     * @return true 如果初始化成功
     */
    virtual bool init_decoder() = 0;

    /**
     * @brief 编码原始音频数据
     *
     * 过程：
     * 1. 接收原始PCM音频数据
     * 2. 使用编码器压缩（AAC、MP3等）
     * 3. 返回压缩后的音频数据
     *
     * @param[in] input 输入的原始PCM数据
     * @param[out] output 输出的编码后数据
     * @return 编码是否成功
     *
     * @note input->data应包含PCM采样数据
     * @note 通常16-bit PCM，每个样本占2字节
     */
    virtual bool encode(const AVFrame& input, AVFrame& output) = 0;

    /**
     * @brief 解码音频比特流
     *
     * 过程：
     * 1. 接收编码的音频数据
     * 2. 使用解码器解码
     * 3. 返回原始PCM数据
     *
     * @param[in] input 输入的编码音频
     * @param[out] output 输出的PCM数据
     * @return 解码是否成功
     */
    virtual bool decode(const AVFrame& input, AVFrame& output) = 0;

    /**
     * @brief 获取编码器类型
     *
     * @return 音频编码格式（AAC、MP3等）
     */
    virtual CodecType get_codec_type() const = 0;

    /**
     * @brief 获取编码器的码率
     *
     * @return 实际输出的比特率
     */
    virtual uint32_t get_bitrate() const = 0;

    /**
     * @brief 设置编码器的码率
     *
     * @param bitrate 新的目标比特率
     * @return 设置是否成功
     */
    virtual bool set_bitrate(uint32_t bitrate) = 0;

    /**
     * @brief 清空缓冲
     */
    virtual void flush() = 0;

    /**
     * @brief 关闭编码/解码器
     */
    virtual void close() = 0;
};

#endif // VIDEO_CODEC_H
