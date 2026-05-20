/*
 * MessageProtocol.h - 自定义TCP消息协议
 *
 * 功能：
 * - 定义网络消息格式
 * - 支持消息序列化和反序列化
 * - 消息类型和控制命令定义
 * - 消息校验和完整性保护
 *
 * 协议设计：
 * 消息结构 = 消息头(Header) + 消息体(Payload)
 *
 * 消息头(20字节固定长度)：
 * [magic:4][type:2][size:4][timestamp:8][crc:2]
 *
 * magic:     0xABCD1234（魔数，用于识别有效消息）
 * type:      消息类型（FRAME_DATA, HEARTBEAT, CONTROL等）
 * size:      消息体大小（字节）
 * timestamp: 消息时间戳（毫秒，用于同步）
 * crc:       消息头的校验和（简单CRC16）
 *
 * 使用场景：
 * - 音视频帧在网络上的传输
 * - 服务器和客户端之间的控制命令
 * - 连接心跳和连接状态管理
 * - 消息流的完整性验证
 */

#ifndef MESSAGE_PROTOCOL_H
#define MESSAGE_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <cstring>
#include <memory>

// ============================================================================
// ======================== 消息类型定义 =======================================
// ============================================================================

/**
 * @enum MessageType
 * @brief 定义所有可能的网络消息类型
 *
 * 消息类型分为三大类：
 * 1. 数据消息（0-99）：传输实际的音视频数据
 * 2. 控制消息（100-199）：控制指令和参数设置
 * 3. 状态消息（200-299）：心跳、确认、错误报告
 */
enum class MessageType : uint16_t {
    // ===== 数据消息 =====
    FRAME_DATA          = 0,    // 音视频帧数据
    VIDEO_FRAME         = 1,    // 视频帧（I帧、P帧、B帧）
    AUDIO_FRAME         = 2,    // 音频帧

    // ===== 控制消息 =====
    START_STREAM        = 100,  // 开始流传输
    STOP_STREAM         = 101,  // 停止流传输
    SET_BITRATE         = 102,  // 设置码率
    SET_QUALITY         = 103,  // 设置质量级别
    CODEC_INFO          = 104,  // 编码器信息

    // ===== 状态消息 =====
    HEARTBEAT           = 200,  // 心跳包
    HEARTBEAT_ACK       = 201,  // 心跳确认
    ACK                 = 202,  // 通用确认
    ERROR               = 203,  // 错误报告
};

/**
 * @enum ErrorCode
 * @brief 错误码定义
 */
enum class ErrorCode : uint8_t {
    SUCCESS             = 0,    // 成功
    INVALID_FORMAT      = 1,    // 无效的消息格式
    CRC_ERROR           = 2,    // 校验和错误
    SIZE_MISMATCH       = 3,    // 大小不匹配
    CODEC_NOT_SUPPORTED = 4,    // 不支持的编码格式
    BUFFER_OVERFLOW     = 5,    // 缓冲区溢出
    UNKNOWN_ERROR       = 255,  // 未知错误
};

// ============================================================================
// ======================== 消息头结构 ========================================
// ============================================================================

/**
 * @struct MessageHeader
 * @brief TCP消息的固定头部结构
 *
 * 设计目的：
 * - 识别消息边界和类型
 * - 验证消息完整性
 * - 支持多种消息类型
 * - 提供基本的时间戳同步
 *
 * 大小：20字节（固定）
 * 对齐方式：4字节对齐
 *
 * 布局（从左到右）：
 * Bytes  0-3:   magic（魔数）
 * Bytes  4-5:   type（消息类型）
 * Bytes  6-9:   payload_size（消息体大小）
 * Bytes 10-17:  timestamp（时间戳）
 * Bytes 18-19:  header_crc（消息头校验和）
 */
struct MessageHeader {
    static constexpr uint32_t MAGIC_NUMBER = 0xABCD1234;  // 用于识别有效消息的魔数
    static constexpr size_t HEADER_SIZE = 20;             // 消息头固定大小

    uint32_t magic;              // [0-3]   魔数，用于同步和消息识别
    uint16_t type;               // [4-5]   消息类型（转换为MessageType枚举）
    uint32_t payload_size;       // [6-9]   消息体（负载）的大小（字节）
    uint64_t timestamp;          // [10-17] 消息时间戳（毫秒）
    uint16_t header_crc;         // [18-19] 消息头的CRC校验和（保护前18字节）

    /**
     * @brief 构造函数 - 初始化消息头
     *
     * @param msg_type 消息类型
     * @param payload_sz 消息体大小
     * @param ts 时间戳（毫秒）
     *
     * @note 构造函数自动设置magic和计算header_crc
     * @note 时间戳通常通过std::chrono获取
     */
    MessageHeader(MessageType msg_type = MessageType::FRAME_DATA,
                  uint32_t payload_sz = 0,
                  uint64_t ts = 0)
        : magic(MAGIC_NUMBER),
          type(static_cast<uint16_t>(msg_type)),
          payload_size(payload_sz),
          timestamp(ts),
          header_crc(0) {
        // 计算消息头的校验和
        header_crc = calculate_crc();
    }

    /**
     * @brief 验证消息头的有效性
     *
     * @return true 如果消息头有效（魔数正确、CRC校验通过）
     *
     * 检查项：
     * 1. 魔数是否为MAGIC_NUMBER
     * 2. CRC校验和是否匹配
     * 3. 消息体大小是否合理（< 100MB）
     */
    bool is_valid() const {
        // 检查魔数
        if (magic != MAGIC_NUMBER) {
            return false;
        }

        // 检查payload_size合理性（防止过大的消息）
        const uint32_t MAX_PAYLOAD_SIZE = 100 * 1024 * 1024;  // 100MB
        if (payload_size > MAX_PAYLOAD_SIZE) {
            return false;
        }

        // 验证CRC
        if (header_crc != calculate_crc()) {
            return false;
        }

        return true;
    }

    /**
     * @brief 计算消息头的CRC校验和
     *
     * @return 计算得到的CRC值
     *
     * @note 校验范围：整个消息头的前18字节（不包含crc字段本身）
     * @note 使用简单的CRC16校验（标准CRC-CCITT）
     */
    uint16_t calculate_crc() const {
        // 简化的CRC16实现
        // 在实际项目中，应该使用标准CRC库或更复杂的校验算法
        uint16_t crc = 0xFFFF;

        // 对前18字节计算CRC
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        for (int i = 0; i < 18; ++i) {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xA001;  // CRC-16多项式
                } else {
                    crc >>= 1;
                }
            }
        }

        return crc;
    }

    /**
     * @brief 将消息头序列化为字节数组
     *
     * @param[out] buffer 输出缓冲区（至少20字节）
     * @return 写入的字节数（总是20）
     *
     * @note 使用网络字节序（大端）序列化
     * @note 确保buffer至少有20字节空间
     */
    size_t serialize(uint8_t* buffer) const {
        if (!buffer) {
            return 0;
        }

        // 使用memcpy进行序列化，对齐性能更好
        // 注意：在实际网络传输中应转换为网络字节序（大端）
        std::memcpy(buffer, this, HEADER_SIZE);
        return HEADER_SIZE;
    }

    /**
     * @brief 从字节数组反序列化消息头
     *
     * @param[in] buffer 输入缓冲区（至少20字节）
     * @return 读取的字节数（总是20，或0如果失败）
     *
     * @note 反序列化后应立即调用is_valid()验证
     */
    size_t deserialize(const uint8_t* buffer) {
        if (!buffer) {
            return 0;
        }

        std::memcpy(this, buffer, HEADER_SIZE);
        return HEADER_SIZE;
    }
};

// ============================================================================
// ======================== 消息类 ==========================================
// ============================================================================

/**
 * @class Message
 * @brief 完整的TCP消息类（消息头 + 消息体）
 *
 * 使用场景：
 * - 编码要发送的消息
 * - 解码接收到的消息
 * - 管理消息的序列化和反序列化
 *
 * 设计特点：
 * - 分离消息头和消息体管理
 * - 自动计算和验证CRC
 * - 支持快速的字节流编码
 *
 * 使用示例：
 * @code
 *   // 创建一个视频帧消息
 *   Message msg(MessageType::VIDEO_FRAME, 1024);
 *   msg.set_payload(frame_data, 1024);
 *
 *   // 序列化为字节流（用于网络发送）
 *   std::vector<uint8_t> bytes = msg.to_bytes();
 *
 *   // 从字节流反序列化
 *   Message received;
 *   received.from_bytes(bytes);
 * @endcode
 */
class Message {
public:
    /**
     * @brief 默认构造函数
     * 创建一个空消息
     */
    Message()
        : header_(MessageType::FRAME_DATA, 0, 0),
          payload_(),
          valid_(true) {
    }

    /**
     * @brief 参数化构造函数
     *
     * @param type 消息类型
     * @param payload_size 预期的消息体大小
     * @param timestamp 时间戳（默认为0）
     */
    explicit Message(MessageType type,
                    uint32_t payload_size = 0,
                    uint64_t timestamp = 0)
        : header_(type, payload_size, timestamp),
          valid_(true) {
        // 预分配消息体缓冲区
        if (payload_size > 0) {
            payload_.reserve(payload_size);
        }
    }

    /**
     * @brief 复制构造函数
     */
    Message(const Message& other)
        : header_(other.header_),
          payload_(other.payload_),
          valid_(other.valid_) {
    }

    /**
     * @brief 赋值操作符
     */
    Message& operator=(const Message& other) {
        if (this != &other) {
            header_ = other.header_;
            payload_ = other.payload_;
            valid_ = other.valid_;
        }
        return *this;
    }

    /**
     * @brief 析构函数
     */
    ~Message() = default;

    // ===== 消息头操作 =====

    /**
     * @brief 获取消息类型
     *
     * @return 消息类型枚举值
     */
    MessageType get_type() const {
        return static_cast<MessageType>(header_.type);
    }

    /**
     * @brief 设置消息类型
     *
     * @param type 新的消息类型
     */
    void set_type(MessageType type) {
        header_.type = static_cast<uint16_t>(type);
        header_.header_crc = header_.calculate_crc();
    }

    /**
     * @brief 获取消息的时间戳
     *
     * @return 时间戳（毫秒）
     */
    uint64_t get_timestamp() const {
        return header_.timestamp;
    }

    /**
     * @brief 设置消息的时间戳
     *
     * @param ts 新的时间戳
     */
    void set_timestamp(uint64_t ts) {
        header_.timestamp = ts;
        header_.header_crc = header_.calculate_crc();
    }

    /**
     * @brief 获取消息头引用
     *
     * @return 常量消息头引用
     *
     * @note 用于需要直接访问消息头的场景
     */
    const MessageHeader& get_header() const {
        return header_;
    }

    // ===== 消息体操作 =====

    /**
     * @brief 设置消息体数据
     *
     * @param[in] data 数据指针
     * @param size 数据大小（字节）
     * @return true 如果设置成功
     *
     * @note 会复制数据到内部缓冲区
     * @note 自动更新消息头中的payload_size
     */
    bool set_payload(const void* data, uint32_t size) {
        if (!data || size == 0) {
            payload_.clear();
            header_.payload_size = 0;
            return true;
        }

        try {
            payload_.resize(size);
            std::memcpy(payload_.data(), data, size);
            header_.payload_size = size;
            return true;
        } catch (...) {
            valid_ = false;
            return false;
        }
    }

    /**
     * @brief 获取消息体数据指针
     *
     * @return 指向消息体数据的指针
     *
     * @note 如果消息体为空，返回nullptr
     */
    const uint8_t* get_payload() const {
        return payload_.empty() ? nullptr : payload_.data();
    }

    /**
     * @brief 获取可修改的消息体数据指针
     *
     * @return 指向消息体数据的可修改指针
     */
    uint8_t* get_payload_mutable() {
        return payload_.empty() ? nullptr : payload_.data();
    }

    /**
     * @brief 获取消息体大小
     *
     * @return 消息体字节数
     */
    uint32_t get_payload_size() const {
        return header_.payload_size;
    }

    /**
     * @brief 向消息体追加数据
     *
     * @param[in] data 要追加的数据
     * @param size 数据大小
     * @return 追加后的总大小，或0如果失败
     */
    uint32_t append_payload(const void* data, uint32_t size) {
        if (!data || size == 0) {
            return header_.payload_size;
        }

        try {
            const uint8_t* src = static_cast<const uint8_t*>(data);
            payload_.insert(payload_.end(), src, src + size);
            header_.payload_size = payload_.size();
            return header_.payload_size;
        } catch (...) {
            valid_ = false;
            return 0;
        }
    }

    /**
     * @brief 清空消息体
     */
    void clear_payload() {
        payload_.clear();
        header_.payload_size = 0;
    }

    // ===== 序列化和反序列化 =====

    /**
     * @brief 将消息序列化为字节数组
     *
     * @return 包含完整消息（头+体）的字节向量
     *
     * 格式：
     * [20字节消息头][payload_size字节消息体]
     *
     * @note 返回的向量包含完整的可传输数据
     */
    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> result;

        // 预分配内存
        size_t total_size = MessageHeader::HEADER_SIZE + payload_.size();
        result.reserve(total_size);

        // 添加消息头
        std::vector<uint8_t> header_bytes(MessageHeader::HEADER_SIZE);
        header_.serialize(header_bytes.data());
        result.insert(result.end(), header_bytes.begin(), header_bytes.end());

        // 添加消息体
        if (!payload_.empty()) {
            result.insert(result.end(), payload_.begin(), payload_.end());
        }

        return result;
    }

    /**
     * @brief 从字节数组反序列化消息
     *
     * @param[in] data 输入数据指针
     * @param size 输入数据大小
     * @return true 如果反序列化成功
     *
     * @note 输入数据应至少包含20字节的消息头
     * @note 反序列化后应检查is_valid()
     */
    bool from_bytes(const uint8_t* data, size_t size) {
        if (!data || size < MessageHeader::HEADER_SIZE) {
            valid_ = false;
            return false;
        }

        // 反序列化消息头
        header_.deserialize(data);

        // 验证消息头
        if (!header_.is_valid()) {
            valid_ = false;
            return false;
        }

        // 检查总大小是否匹配
        size_t expected_total = MessageHeader::HEADER_SIZE + header_.payload_size;
        if (size < expected_total) {
            valid_ = false;
            return false;
        }

        // 提取消息体
        if (header_.payload_size > 0) {
            const uint8_t* payload_ptr = data + MessageHeader::HEADER_SIZE;
            try {
                payload_.assign(payload_ptr, payload_ptr + header_.payload_size);
            } catch (...) {
                valid_ = false;
                return false;
            }
        } else {
            payload_.clear();
        }

        valid_ = true;
        return true;
    }

    /**
     * @brief 从向量反序列化消息（便利函数）
     *
     * @param[in] data 包含消息数据的向量
     * @return true 如果反序列化成功
     */
    bool from_bytes(const std::vector<uint8_t>& data) {
        return from_bytes(data.data(), data.size());
    }

    // ===== 消息验证 =====

    /**
     * @brief 检查消息是否有效
     *
     * @return true 如果消息有效（格式正确、校验通过）
     */
    bool is_valid() const {
        return valid_ && header_.is_valid();
    }

    /**
     * @brief 获取完整消息的总大小
     *
     * @return 消息头大小 + 消息体大小（字节）
     */
    size_t total_size() const {
        return MessageHeader::HEADER_SIZE + payload_.size();
    }

    /**
     * @brief 获取消息的调试信息字符串
     *
     * @return 包含消息类型、大小、时间戳等信息的字符串
     *
     * @note 用于日志输出和调试
     */
    std::string to_string() const {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer),
                     "Message[type=%u, payload_size=%u, timestamp=%llu, total_size=%zu]",
                     header_.type, header_.payload_size, header_.timestamp, total_size());
        return std::string(buffer);
    }

private:
    MessageHeader header_;           // 消息头
    std::vector<uint8_t> payload_;   // 消息体
    bool valid_;                     // 消息有效性标志
};

// ============================================================================
// ======================== 消息编解码工具 =====================================
// ============================================================================

/**
 * @class ProtocolHelper
 * @brief 提供协议相关的辅助函数
 *
 * 功能：
 * - 获取消息类型的字符串表示
 * - 错误码转换
 * - 时间戳获取
 * - 字节序转换（如果需要）
 */
class ProtocolHelper {
public:
    /**
     * @brief 获取消息类型的字符串描述
     *
     * @param type 消息类型
     * @return 消息类型的字符串名称
     */
    static const char* message_type_to_string(MessageType type) {
        switch (type) {
            case MessageType::FRAME_DATA:      return "FRAME_DATA";
            case MessageType::VIDEO_FRAME:     return "VIDEO_FRAME";
            case MessageType::AUDIO_FRAME:     return "AUDIO_FRAME";
            case MessageType::START_STREAM:    return "START_STREAM";
            case MessageType::STOP_STREAM:     return "STOP_STREAM";
            case MessageType::SET_BITRATE:     return "SET_BITRATE";
            case MessageType::SET_QUALITY:     return "SET_QUALITY";
            case MessageType::CODEC_INFO:      return "CODEC_INFO";
            case MessageType::HEARTBEAT:       return "HEARTBEAT";
            case MessageType::HEARTBEAT_ACK:   return "HEARTBEAT_ACK";
            case MessageType::ACK:             return "ACK";
            case MessageType::ERROR:           return "ERROR";
            default:                           return "UNKNOWN";
        }
    }

    /**
     * @brief 获取错误码的字符串描述
     *
     * @param code 错误码
     * @return 错误码的字符串描述
     */
    static const char* error_code_to_string(ErrorCode code) {
        switch (code) {
            case ErrorCode::SUCCESS:             return "SUCCESS";
            case ErrorCode::INVALID_FORMAT:      return "INVALID_FORMAT";
            case ErrorCode::CRC_ERROR:           return "CRC_ERROR";
            case ErrorCode::SIZE_MISMATCH:       return "SIZE_MISMATCH";
            case ErrorCode::CODEC_NOT_SUPPORTED: return "CODEC_NOT_SUPPORTED";
            case ErrorCode::BUFFER_OVERFLOW:     return "BUFFER_OVERFLOW";
            case ErrorCode::UNKNOWN_ERROR:       return "UNKNOWN_ERROR";
            default:                             return "UNKNOWN";
        }
    }

    /**
     * @brief 获取当前系统时间戳（毫秒）
     *
     * @return 当前时间的毫秒时间戳
     *
     * @note 用于设置Message的timestamp字段
     * @note 基于system_clock（相对于系统启动或1970年）
     */
    static uint64_t get_timestamp_ms() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto duration = now.time_since_epoch();
        return duration_cast<milliseconds>(duration).count();
    }
};

#endif // MESSAGE_PROTOCOL_H
