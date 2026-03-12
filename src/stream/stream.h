

#ifndef __LATTE_STREAM_H
#define __LATTE_STREAM_H

#include <stdio.h>
#include <stdlib.h>
#include <rax/rax.h>
#include "sds/sds.h"

/**
 * @brief Stream 消息 ID：由毫秒时间戳 + 序列号组成的 128 位唯一标识
 *
 * 同一毫秒内生成的 ID 使用相同的毫秒时间戳，序列号递增。
 * 若时钟回拨，则使用上次最新 ID 的毫秒时间并递增序列号。
 */
typedef struct stream_id_t {
    uint64_t ms;  /**< Unix 时间戳（毫秒） */
    uint64_t seq; /**< 序列号，同一毫秒内递增 */
} stream_id_t;

/**
 * @brief Stream 主结构体
 *
 * Redis Stream 数据结构，底层使用 rax（基数树）存储消息，
 * 支持消费者组（cgroups）管理。
 */
typedef struct stream_t {
    rax *rax;                           /**< 基数树，存储所有消息（key=ID, value=消息内容） */
    uint64_t length;                    /**< 当前 Stream 中的消息总数 */
    stream_id_t last_id;                /**< 最后写入的消息 ID（无消息时为 0-0） */
    stream_id_t first_id;               /**< 第一条非墓碑消息的 ID（Stream 为空时为 0-0） */
    stream_id_t max_deleted_entry_id;   /**< 曾被删除的最大消息 ID（用于一致性检测） */
    uint64_t entries_added;             /**< 自 Stream 创建以来累计写入的消息总数 */
    rax *cgroups;                       /**< 消费者组字典：name -> streamCG */
} stream_t;

/**
 * @brief 创建一个新的 Stream 对象
 * @return stream_t* 新建 Stream 指针，失败返回 NULL
 */
stream_t* stream_new();

/**
 * @brief 销毁 Stream 对象及其内部资源
 * @param stream 目标 Stream 指针
 */
void stream_delete(stream_t*);

/**
 * @brief Stream 消费者组（Consumer Group）结构体
 *
 * 记录消费者组的投递进度、未确认消息列表（PEL）和成员信息。
 */
typedef struct stream_cg_t {
    stream_id_t last_id;    /**< 该组最后投递（但未确认）的消息 ID；
                                 新消费者请求消息时只返回 ID > last_id 的消息 */
    long long entries_read; /**< 该组已读取的消息总数
                                 （理想情况下 = 从 0-0 开始且无删除/SETID 操作时的准确值） */
    rax *pel;               /**< 挂起消息列表（Pending Entry List）：
                                 key = 消息 ID（64 位大端格式），
                                 value = streamNACK 结构体；
                                 存储所有已投递但未确认的消息 */
    rax *consumers;         /**< 消费者字典：name -> streamConsumer 结构体 */
} stream_cg_t;

/** @brief 毫秒时间戳类型 */
typedef long long mstime_t;
/** @brief 微秒时间戳类型 */
typedef long long ustime_t;

/**
 * @brief Stream 消费者结构体
 *
 * 代表消费者组中的单个消费者，记录最近活跃时间及其专属 PEL。
 */
typedef struct stream_consumer_t {
    mstime_t seen_time;   /**< 消费者最后一次尝试读取/认领消息的时间（毫秒） */
    mstime_t active_time; /**< 消费者最后一次成功读取/认领消息的时间（毫秒） */
    sds name;             /**< 消费者名称（大小写敏感，用于消费者组协议标识） */
    rax *pel;             /**< 该消费者专属的挂起消息列表：
                               key = 消息 ID（大端格式），
                               value = 与消费者组 PEL 共享的 streamNACK 指针 */
} stream_consumer_t;

/**
 * @brief Stream 未确认消息（NACK）结构体
 *
 * 记录一条已投递但尚未被消费者确认（ACK）的消息的元数据。
 */
typedef struct stream_nack_t {
    mstime_t delivery_time;         /**< 最后一次投递该消息的时间（毫秒） */
    uint64_t delivery_count;        /**< 该消息累计投递次数 */
    stream_consumer_t *consumer;    /**< 最后一次接收该消息的消费者指针 */
} stream_nack_t;

#endif
