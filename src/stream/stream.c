
/**
 * @file stream.c
 * @brief Stream流数据结构实现文件
 * @details 实现了消息流数据结构，支持消费者组和消息确认机制
 * @author latte_c
 * @date 2026-03-12
 */

#include "stream.h"
#include "zmalloc/zmalloc.h"
#include "rax/rax.h"
#include "list/listpack.h"
/**
 * @brief 创建一个新的 Stream 对象，初始化所有字段为零值
 * cgroups 字段延迟创建（首次使用时按需分配），节省内存。
 * @return stream_t* 新建 Stream 指针
 */
stream_t* stream_new() {
    stream_t *s = zmalloc(sizeof(*s));
    s->rax = raxNew();         /* 创建底层基数树，用于存储消息 */
    s->length = 0;
    s->first_id.ms = 0;
    s->first_id.seq = 0;
    s->last_id.ms = 0;
    s->last_id.seq = 0;
    s->max_deleted_entry_id.seq = 0;
    s->max_deleted_entry_id.ms = 0;
    s->entries_added = 0;
    s->cgroups = NULL; /* 按需创建消费者组字典，避免空 Stream 浪费内存 */
    return s;
}

/**
 * @brief 释放一个 NACK（未确认消息）条目
 * @param na 要释放的 NACK 结构体指针
 */
void stream_free_nack(stream_nack_t *na) {
    zfree(na);
}

/**
 * @brief 释放单个消费者及其专属 PEL
 * 注意：本函数仅释放消费者自身的 PEL 树结构（不释放 PEL 中 NACK 的值，
 * 因为 NACK 与消费者组的 PEL 共享），释放消费者名称和结构体本身。
 * 调用方在删除消费者（而非销毁整个 Stream）时，
 * 需提前处理该消费者的未确认消息。
 * @param sc 要释放的消费者结构体指针
 */
void stream_free_consumer(stream_consumer_t *sc) {
    raxFree(sc->pel); /* 仅释放树结构，PEL 节点的值由消费者组共享，不在此释放 */
    sds_delete(sc->name);
    zfree(sc);
}

/**
 * @brief 释放一个消费者组及其所有关联资源
 * 依次释放：组级 PEL（含 NACK 值）、消费者字典（含各消费者结构体）、组本身。
 * @param cg 要释放的消费者组结构体指针
 */
void stream_free_cg(stream_cg_t *cg) {
    /* 释放组级 PEL，同时调用 stream_free_nack 释放每个 NACK 结构 */
    raxFreeWithCallback(cg->pel,(void(*)(void*))stream_free_nack);
    /* 释放消费者字典，同时调用 stream_free_consumer 释放每个消费者 */
    raxFreeWithCallback(cg->consumers,(void(*)(void*))stream_free_consumer);
    zfree(cg);
}

/**
 * @brief 销毁 Stream 对象及其全部内部资源
 * 释放顺序：rax 消息树（含 listpack 节点）→ cgroups（若存在）→ Stream 本身。
 * @param s 目标 Stream 指针
 */
void stream_delete(stream_t* s) {
    /* 释放 rax 树中的每个 listpack 节点 */
    raxFreeWithCallback(s->rax, (void(*)(void*))lp_free);
    /* 若消费者组字典存在，则释放所有消费者组 */
    if (s->cgroups)
        raxFreeWithCallback(s->cgroups, (void(*)(void*))stream_free_cg);
    zfree(s);
}