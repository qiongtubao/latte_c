/**
 * @file raft_test.c
 * @brief Raft 分布式一致性协议单元测试
 * 
 * 本文件包含 Raft 协议的所有单元测试，测试覆盖：
 * - 节点创建和初始化
 * - 日志操作（追加、查询、删除）
 * - 状态转换（Follower、Candidate、Leader）
 * - RPC 处理（RequestVote、AppendEntries）
 * - 选举机制
 * - 日志复制
 * - 节点管理
 * - 配置管理
 * - 时间驱动函数
 */

/* 头文件包含 */
#include "raft.h"                    /* Raft 协议头文件 */
#include "../test/testhelp.h"        /* 测试辅助宏（test_cond、test_report） */
#include "../test/testassert.h"      /* 测试断言宏 */
#include "utils/monotonic.h"         /* 单调时钟，用于时间相关测试 */
#include <stdio.h>                   /* 标准输入输出 */
#include <string.h>                  /* 字符串操作函数 */
#include <stdlib.h>                  /* 标准库函数（rand 等） */
#include <time.h>                    /* 时间相关函数 */

/* ==================== Mock RPC 处理函数（用于测试） ==================== */

/**
 * @brief Mock RequestVote RPC 发送函数
 * 
 * 在真实实现中，这会通过网络发送 RPC 请求。
 * 在测试中，我们模拟响应：总是授予投票。
 * 
 * @param node 节点指针（未使用）
 * @param peer_id 对等节点 ID（未使用）
 * @param args 请求参数（未使用）
 * @param result 响应结果（输出参数）
 * @return RAFT_OK 成功
 */
static int mock_send_request_vote(raft_node_t *node, uint64_t peer_id,
                                   raft_request_vote_args_t *args,
                                   raft_request_vote_result_t *result) {
    /* 在真实实现中，这会通过网络发送 RPC 请求 */
    /* 在测试中，我们模拟响应：总是授予投票 */
    (void)node;      /* 标记未使用的参数，避免编译警告 */
    (void)peer_id;   /* 标记未使用的参数 */
    (void)args;      /* 标记未使用的参数 */
    if (result) {    /* 如果结果指针有效 */
        result->term = args->term;        /* 设置响应任期（与请求任期相同） */
        result->vote_granted = true;     /* 授予投票（用于测试） */
    }
    return RAFT_OK;  /* 返回成功 */
}

/**
 * @brief Mock AppendEntries RPC 发送函数
 * 
 * 在真实实现中，这会通过网络发送 RPC 请求。
 * 在测试中，我们模拟响应：总是成功。
 * 
 * @param node 节点指针
 * @param peer_id 对等节点 ID（未使用）
 * @param args 请求参数（未使用）
 * @param result 响应结果（输出参数）
 * @return RAFT_OK 成功
 */
static int mock_send_append_entries(raft_node_t *node, uint64_t peer_id,
                                      raft_append_entries_args_t *args,
                                      raft_append_entries_result_t *result) {
    /* 在真实实现中，这会通过网络发送 RPC 请求 */
    /* 在测试中，我们模拟响应：总是成功 */
    (void)peer_id;   /* 标记未使用的参数 */
    (void)args;      /* 标记未使用的参数 */
    if (result) {    /* 如果结果指针有效 */
        result->term = node->persistent->current_term;  /* 设置响应任期 */
        result->success = true;                         /* 操作成功 */
    }
    return RAFT_OK;  /* 返回成功 */
}

/**
 * @brief Mock 日志条目应用函数
 * 
 * 在真实实现中，这会将日志条目应用到状态机器。
 * 在测试中，我们只是返回成功。
 * 
 * @param node 节点指针（未使用）
 * @param entry 日志条目（未使用）
 * @return RAFT_OK 成功
 */
static int mock_apply_log_entry(raft_node_t *node, raft_log_entry_t *entry) {
    /* 在真实实现中，这会将日志条目应用到状态机器 */
    (void)node;   /* 标记未使用的参数 */
    (void)entry;  /* 标记未使用的参数 */
    return RAFT_OK;  /* 返回成功 */
}

/**
 * @brief Mock 状态持久化函数
 * 
 * 在真实实现中，这会将状态持久化到磁盘。
 * 在测试中，我们只是返回成功。
 * 
 * @param node 节点指针（未使用）
 * @return RAFT_OK 成功
 */
static int mock_persist_state(raft_node_t *node) {
    /* 在真实实现中，这会将状态持久化到磁盘 */
    (void)node;  /* 标记未使用的参数 */
    return RAFT_OK;  /* 返回成功 */
}

/**
 * @brief Mock 状态加载函数
 * 
 * 在真实实现中，这会从磁盘加载状态。
 * 在测试中，我们只是返回成功。
 * 
 * @param node 节点指针（未使用）
 * @return RAFT_OK 成功
 */
static int mock_load_state(raft_node_t *node) {
    /* 在真实实现中，这会从磁盘加载状态 */
    (void)node;  /* 标记未使用的参数 */
    return RAFT_OK;  /* 返回成功 */
}

/* ==================== 测试函数 ==================== */

/**
 * @brief 测试节点创建和初始化
 * 
 * 测试 Raft 节点的创建和初始化过程，验证：
 * - 节点可以成功创建
 * - 节点 ID 正确设置
 * - 初始状态为 FOLLOWER
 * - 初始任期号为 0
 * - 持久化状态和易失状态正确初始化
 * - 日志包含初始空条目（索引 0）
 * 
 * @return 1 测试通过，0 测试失败
 */
int test_raft_node_create() {
    /* 创建节点，节点 ID 为 1 */
    raft_node_t *node = raft_node_create(1);
    test_cond("Create raft node", node != NULL);  /* 验证节点创建成功 */
    
    if (node) {  /* 如果节点创建成功 */
        /* 验证节点 ID 正确设置 */
        test_cond("Node ID is set", node->node_id == 1);
        
        /* 验证初始状态为 FOLLOWER */
        test_cond("Initial state is FOLLOWER", node->state == RAFT_STATE_FOLLOWER);
        
        /* 验证初始任期号为 0 */
        test_cond("Initial term is 0", raft_get_current_term(node) == 0);
        
        /* 验证持久化状态存在 */
        test_cond("Persistent state exists", node->persistent != NULL);
        
        /* 验证易失状态存在 */
        test_cond("Volatile state exists", node->volatile_state != NULL);
        
        /* 验证初始日志包含索引 0 的条目 */
        test_cond("Initial log has entry at index 0", raft_log_get_last_index(node) == 0);
        
        /* 清理：删除节点并释放所有资源 */
        raft_node_delete(node);
    }
    
    return 1;  /* 测试通过 */
}

/**
 * @brief 测试日志操作
 * 
 * 测试日志的各种操作，包括：
 * - 追加日志条目
 * - 获取日志条目
 * - 获取最后一条日志的索引和任期
 * - 删除日志条目
 * 
 * @return 1 测试通过，0 测试失败
 */
int test_raft_log_operations() {
    /* 创建测试节点 */
    raft_node_t *node = raft_node_create(1);
    if (!node) return 0;  /* 节点创建失败，测试失败 */
    
    /* 追加日志条目 */
    sds_t data1 = sds_new("command1");  /* 创建第一个命令数据 */
    sds_t data2 = sds_new("command2");  /* 创建第二个命令数据 */
    
    /* 测试追加第一条日志条目 */
    test_cond("Append first log entry", 
              raft_log_append(node, data1, sds_len(data1)) == RAFT_OK);
    
    /* 测试追加第二条日志条目 */
    test_cond("Append second log entry", 
              raft_log_append(node, data2, sds_len(data2)) == RAFT_OK);
    
    /* 验证最后一条日志的索引为 2 */
    test_cond("Last log index is 2", raft_log_get_last_index(node) == 2);
    
    /* 验证最后一条日志的任期与当前任期匹配 */
    test_cond("Last log term matches current term", 
              raft_log_get_last_term(node) == raft_get_current_term(node));
    
    /* 获取日志条目 */
    raft_log_entry_t *entry1 = raft_log_get(node, 1);  /* 获取索引 1 的条目 */
    raft_log_entry_t *entry2 = raft_log_get(node, 2);  /* 获取索引 2 的条目 */
    
    /* 验证可以获取日志条目 */
    test_cond("Get log entry at index 1", entry1 != NULL);
    test_cond("Get log entry at index 2", entry2 != NULL);
    
    /* 验证日志条目的数据正确 */
    if (entry1) {
        test_cond("Entry 1 has correct data", 
                  strcmp(entry1->data, "command1") == 0);  /* 验证第一条数据 */
    }
    if (entry2) {
        test_cond("Entry 2 has correct data", 
                  strcmp(entry2->data, "command2") == 0);  /* 验证第二条数据 */
    }
    
    /* 测试日志删除 */
    /* 从索引 2 开始删除（会删除索引 2 及之后的所有条目） */
    test_cond("Delete log from index 2", 
              raft_log_delete_from(node, 2) == RAFT_OK);
    
    /* 验证删除后最后一条日志的索引为 1 */
    test_cond("Last log index is 1 after deletion", 
              raft_log_get_last_index(node) == 1);
    
    /* 清理资源 */
    sds_delete(data1);  /* 释放第一个数据字符串 */
    sds_delete(data2);  /* 释放第二个数据字符串 */
    raft_node_delete(node);  /* 删除节点并释放所有资源 */
    
    return 1;  /* 测试通过 */
}

/**
 * @brief 测试状态转换
 * 
 * 测试节点在不同状态之间的转换：
 * - Follower → Candidate → Leader → Follower
 * 验证每次状态转换时相关字段的正确更新。
 * 
 * @return 1 测试通过，0 测试失败
 */
int test_raft_state_transitions() {
    /* 创建测试节点 */
    raft_node_t *node = raft_node_create(1);
    if (!node) return 0;  /* 节点创建失败，测试失败 */
    
    /* 初始状态为 Follower */
    test_cond("Initial state is FOLLOWER", 
              raft_get_state(node) == RAFT_STATE_FOLLOWER);  /* 验证初始状态 */
    test_cond("Not leader initially", !raft_is_leader(node));  /* 验证不是 Leader */
    
    /* 转换为 Candidate */
    test_cond("Become candidate", 
              raft_become_candidate(node) == RAFT_OK);  /* 验证转换成功 */
    test_cond("State is CANDIDATE", 
              raft_get_state(node) == RAFT_STATE_CANDIDATE);  /* 验证状态已改变 */
    test_cond("Term increased", raft_get_current_term(node) == 1);  /* 验证任期增加（从 0 到 1） */
    test_cond("Voted for self", node->persistent->voted_for == node->node_id);  /* 验证投票给自己 */
    
    /* 转换为 Leader（模拟赢得选举） */
    test_cond("Become leader", 
              raft_become_leader(node) == RAFT_OK);  /* 验证转换成功 */
    test_cond("State is LEADER", 
              raft_get_state(node) == RAFT_STATE_LEADER);  /* 验证状态为 Leader */
    test_cond("Is leader", raft_is_leader(node));  /* 验证 is_leader 函数返回 true */
    test_cond("Leader state exists", node->leader_state != NULL);  /* 验证 Leader 状态已创建 */
    
    /* 转换为 Follower */
    test_cond("Become follower", 
              raft_become_follower(node, 2) == RAFT_OK);  /* 转换为 Follower，任期更新为 2 */
    test_cond("State is FOLLOWER", 
              raft_get_state(node) == RAFT_STATE_FOLLOWER);  /* 验证状态为 Follower */
    test_cond("Term updated", raft_get_current_term(node) == 2);  /* 验证任期已更新为 2 */
    test_cond("Not leader after becoming follower", !raft_is_leader(node));  /* 验证不再是 Leader */
    
    /* 清理资源 */
    raft_node_delete(node);
    
    return 1;  /* 测试通过 */
}

/**
 * @brief 测试 RequestVote RPC 处理
 * 
 * 测试投票请求的处理逻辑，包括：
 * - 授予投票（当任期更高且日志足够新时）
 * - 拒绝投票（当已投票给其他候选者时）
 * - 拒绝投票（当请求任期更低时）
 * 
 * @return 1 测试通过，0 测试失败
 */
int test_raft_request_vote() {
    /* 创建测试节点 */
    raft_node_t *node = raft_node_create(1);
    if (!node) return 0;  /* 节点创建失败，测试失败 */
    
    /* 添加一些日志条目（用于测试日志比较逻辑） */
    sds_t data = sds_new("test");  /* 创建测试数据 */
    raft_log_append(node, data, sds_len(data));  /* 追加到日志 */
    sds_delete(data);  /* 释放数据（日志中已复制） */
    
    /* 创建 RequestVote 请求参数 */
    raft_request_vote_args_t args;
    args.term = 1;                                    /* 候选者的任期（大于节点的当前任期 0） */
    args.candidate_id = 2;                          /* 候选者的节点 ID */
    args.last_log_index = raft_log_get_last_index(node);  /* 候选者最后一条日志的索引 */
    args.last_log_term = raft_log_get_last_term(node);    /* 候选者最后一条日志的任期 */
    
    raft_request_vote_result_t result;  /* 投票响应结果 */
    
    /* 测试：当任期更高时授予投票 */
    test_cond("Handle RequestVote", 
              raft_handle_request_vote(node, &args, &result) == RAFT_OK);  /* 验证处理成功 */
    test_cond("Vote granted", result.vote_granted == true);  /* 验证投票被授予 */
    test_cond("Became follower", node->state == RAFT_STATE_FOLLOWER);  /* 验证转换为 Follower */
    test_cond("Term updated", raft_get_current_term(node) == 1);  /* 验证任期已更新 */
    test_cond("Voted for candidate", node->persistent->voted_for == 2);  /* 验证投票给候选者 2 */
    
    /* 测试：当已投票时拒绝投票 */
    args.term = 1;        /* 相同任期 */
    args.candidate_id = 3;  /* 不同的候选者 */
    raft_handle_request_vote(node, &args, &result);  /* 处理投票请求 */
    test_cond("Vote not granted when already voted", 
              result.vote_granted == false);  /* 验证投票未被授予（已投票给候选者 2） */
    
    /* 测试：当请求任期更低时拒绝投票 */
    args.term = 0;        /* 更低的任期（节点的当前任期是 1） */
    args.candidate_id = 4;  /* 另一个候选者 */
    raft_handle_request_vote(node, &args, &result);  /* 处理投票请求 */
    test_cond("Vote not granted for lower term", 
              result.vote_granted == false);  /* 验证投票未被授予（任期过低） */
    
    /* 清理资源 */
    raft_node_delete(node);
    
    return 1;  /* 测试通过 */
}

/**
 * @brief 测试 AppendEntries RPC 处理
 * 
 * 测试日志复制请求的处理逻辑，包括：
 * - 接受心跳（空的 AppendEntries）
 * - 接受并追加新的日志条目
 * 
 * @return 1 测试通过，0 测试失败
 */
int test_raft_append_entries() {
    /* 创建测试节点 */
    raft_node_t *node = raft_node_create(1);
    if (!node) return 0;  /* 节点创建失败，测试失败 */
    
    /* 创建 AppendEntries 请求参数（心跳：entries 为空） */
    raft_append_entries_args_t args;
    args.term = 1;              /* Leader 的任期（大于节点的当前任期 0） */
    args.leader_id = 2;        /* Leader 的节点 ID */
    args.prev_log_index = 0;    /* 前一条日志的索引（0 表示从日志开头开始） */
    args.prev_log_term = 0;     /* 前一条日志的任期 */
    args.entries = list_new();  /* 日志条目列表（空列表表示心跳） */
    args.leader_commit = 0;      /* Leader 的提交索引 */
    
    raft_append_entries_result_t result;  /* 响应结果 */
    
    /* 测试：接受心跳 */
    test_cond("Handle AppendEntries (heartbeat)", 
              raft_handle_append_entries(node, &args, &result) == RAFT_OK);  /* 验证处理成功 */
    test_cond("AppendEntries successful", result.success == true);  /* 验证操作成功 */
    test_cond("Became follower", node->state == RAFT_STATE_FOLLOWER);  /* 验证转换为 Follower */
    test_cond("Term updated", raft_get_current_term(node) == 1);  /* 验证任期已更新 */
    
    /* 测试：追加新条目 */
    sds_t data = sds_new("new_command");  /* 创建新命令数据 */
    raft_log_entry_t *entry = raft_log_entry_create(1, 1, data, sds_len(data));  /* 创建日志条目 */
    list_add_node_tail(args.entries, entry);  /* 将条目添加到请求列表 */
    args.prev_log_index = 0;  /* 前一条日志索引（从日志开头开始） */
    args.prev_log_term = 0;   /* 前一条日志任期 */
    
    raft_handle_append_entries(node, &args, &result);  /* 处理日志复制请求 */
    test_cond("AppendEntries with new entry", result.success == true);  /* 验证操作成功 */
    test_cond("Log entry added", raft_log_get_last_index(node) == 1);  /* 验证日志条目已添加 */
    
    /* 清理资源 */
    list_delete(args.entries);  /* 删除条目列表（注意：列表中的条目指针指向已复制的日志，不需要释放） */
    sds_delete(data);           /* 释放原始数据字符串 */
    raft_node_delete(node);     /* 删除节点并释放所有资源 */
    
    return 1;  /* 测试通过 */
}

/* Test election */
int test_raft_election() {
    raft_node_t *node = raft_node_create(1);
    if (!node) return 0;
    
    /* Add peers */
    raft_add_peer(node, 2);
    raft_add_peer(node, 3);
    
    /* Set up mock RPC handler */
    node->send_request_vote = mock_send_request_vote;
    
    /* Start election - node should become candidate first */
    test_cond("Start election", 
              raft_start_election(node) == RAFT_OK);
    /* After start_election, node should be candidate or leader (if won) */
    test_cond("Became candidate or leader", 
              node->state == RAFT_STATE_CANDIDATE || node->state == RAFT_STATE_LEADER);
    /* If became leader, term was already increased in become_candidate */
    if (node->state == RAFT_STATE_CANDIDATE) {
        test_cond("Term increased", 
                  raft_get_current_term(node) >= 1);
        test_cond("Voted for self", 
                  node->persistent->voted_for == node->node_id);
        test_cond("Votes received >= 1", 
                  node->votes_received >= 1);
    } else {
        /* If became leader, these should still be true */
        test_cond("Term increased (leader)", 
                  raft_get_current_term(node) >= 1);
        test_cond("Voted for self (leader)", 
                  node->persistent->voted_for == node->node_id);
    }
    
    raft_node_delete(node);
    
    return 1;
}

/**
 * @brief 测试日志复制
 * 
 * 测试 Leader 的日志复制功能：
 * - 节点成为 Leader
 * - 添加对等节点
 * - 复制日志条目到所有 Follower
 * 
 * @return 1 测试通过，0 测试失败
 */
int test_raft_log_replication() {
    /* 创建测试节点 */
    raft_node_t *node = raft_node_create(1);
    if (!node) return 0;  /* 节点创建失败，测试失败 */
    
    /* 转换为 Leader（模拟赢得选举） */
    raft_become_leader(node);  /* 将节点转换为 Leader 状态 */
    
    /* 添加对等节点（模拟 3 节点集群） */
    raft_add_peer(node, 2);  /* 添加节点 2 */
    raft_add_peer(node, 3);  /* 添加节点 3 */
    
    /* 设置 Mock RPC 处理器 */
    node->send_append_entries = mock_send_append_entries;  /* 设置日志复制回调 */
    
    /* 复制日志条目 */
    sds_t data = sds_new("replicated_command");  /* 创建要复制的命令数据 */
    test_cond("Replicate log entry", 
              raft_replicate_log(node, data, sds_len(data)) == RAFT_OK);  /* 验证复制成功 */
    test_cond("Log entry added", 
              raft_log_get_last_index(node) == 1);  /* 验证日志条目已添加到本地日志 */
    
    /* 清理资源 */
    sds_delete(data);       /* 释放数据字符串 */
    raft_node_delete(node); /* 删除节点并释放所有资源 */
    
    return 1;  /* 测试通过 */
}

/**
 * @brief 测试节点管理
 * 
 * 测试对等节点的添加和移除功能：
 * - 添加多个对等节点
 * - 验证节点数量
 * - 添加重复节点（应该成功，但不增加数量）
 * - 移除节点
 * - 移除不存在的节点（应该失败）
 * 
 * @return 1 测试通过，0 测试失败
 */
int test_raft_peer_management() {
    /* 创建测试节点 */
    raft_node_t *node = raft_node_create(1);
    if (!node) return 0;  /* 节点创建失败，测试失败 */
    
    /* 添加对等节点 */
    test_cond("Add peer 2", raft_add_peer(node, 2) == RAFT_OK);  /* 添加节点 2 */
    test_cond("Add peer 3", raft_add_peer(node, 3) == RAFT_OK);  /* 添加节点 3 */
    test_cond("Add peer 4", raft_add_peer(node, 4) == RAFT_OK);  /* 添加节点 4 */
    test_cond("Peer count is 3", list_length(node->peers) == 3);  /* 验证节点数量为 3 */
    
    /* 尝试添加重复节点（应该成功，但不增加数量） */
    test_cond("Add duplicate peer (should succeed)", 
              raft_add_peer(node, 2) == RAFT_OK);  /* 再次添加节点 2，应该成功但不增加数量 */
    
    /* 移除节点 */
    test_cond("Remove peer 3", raft_remove_peer(node, 3) == RAFT_OK);  /* 移除节点 3 */
    test_cond("Peer count is 2 after removal", 
              list_length(node->peers) == 2);  /* 验证节点数量减少为 2 */
    
    /* 移除不存在的节点（应该失败） */
    test_cond("Remove non-existent peer", 
              raft_remove_peer(node, 99) == RAFT_ERR);  /* 尝试移除不存在的节点 99，应该失败 */
    
    /* 清理资源 */
    raft_node_delete(node);
    
    return 1;  /* 测试通过 */
}

/**
 * @brief 测试配置管理
 * 
 * 测试节点的配置功能：
 * - 设置选举超时时间
 * - 设置心跳间隔时间
 * 
 * @return 1 测试通过，0 测试失败
 */
int test_raft_configuration() {
    /* 创建测试节点 */
    raft_node_t *node = raft_node_create(1);
    if (!node) return 0;  /* 节点创建失败，测试失败 */
    
    /* 设置超时时间 */
    raft_set_election_timeout(node, 200);      /* 设置选举超时为 200 毫秒 */
    raft_set_heartbeat_interval(node, 50);     /* 设置心跳间隔为 50 毫秒 */
    
    /* 验证配置已设置 */
    test_cond("Election timeout set", node->election_timeout == 200);  /* 验证选举超时 */
    test_cond("Heartbeat interval set", node->heartbeat_interval == 50);  /* 验证心跳间隔 */
    
    /* 清理资源 */
    raft_node_delete(node);
    
    return 1;  /* 测试通过 */
}

/**
 * @brief 测试 tick 函数
 * 
 * 测试时间驱动函数的功能：
 * - 作为 Follower 时的 tick（检查选举超时）
 * - 选举超时后触发选举
 * 
 * @return 1 测试通过，0 测试失败
 */
int test_raft_tick() {
    /* 创建测试节点 */
    raft_node_t *node = raft_node_create(1);
    if (!node) return 0;  /* 节点创建失败，测试失败 */
    
    /* 确保单调时钟已初始化 */
    if (getMonotonicUs == NULL) {  /* 如果时钟未初始化 */
        monotonicInit();            /* 初始化单调时钟 */
    }
    uint64_t current_time = getMonotonicUs() / 1000;  /* 获取当前时间（毫秒） */
    
    /* 测试：作为 Follower 时的 tick */
    test_cond("Tick as follower", 
              raft_tick(node, current_time) == RAFT_OK);  /* 验证 tick 函数执行成功 */
    
    /* 将选举截止时间设置为过去，以触发选举 */
    node->election_deadline = current_time - 1;  /* 设置截止时间为过去（已超时） */
    raft_tick(node, current_time);  /* 执行 tick，应该触发选举 */
    test_cond("Election triggered when deadline passed", 
              node->state == RAFT_STATE_CANDIDATE || 
              node->state == RAFT_STATE_LEADER);  /* 验证状态已转换为 Candidate 或 Leader */
    
    /* 清理资源 */
    raft_node_delete(node);
    
    return 1;  /* 测试通过 */
}

/* ==================== 主测试函数 ==================== */

/**
 * @brief 主测试函数
 * 
 * 运行所有测试用例并生成测试报告。
 * 每个测试用例都会验证 Raft 协议的特定功能。
 * 
 * @return 1 所有测试通过，0 有测试失败
 */
int test_api(void) {
    /* 测试用例 1：节点创建和初始化 */
    {
        test_cond("Raft node creation", test_raft_node_create() == 1);
    }
    
    /* 测试用例 2：日志操作 */
    {
        test_cond("Raft log operations", test_raft_log_operations() == 1);
    }
    
    /* 测试用例 3：状态转换 */
    {
        test_cond("Raft state transitions", test_raft_state_transitions() == 1);
    }
    
    /* 测试用例 4：RequestVote RPC 处理 */
    {
        test_cond("Raft RequestVote RPC", test_raft_request_vote() == 1);
    }
    
    /* 测试用例 5：AppendEntries RPC 处理 */
    {
        test_cond("Raft AppendEntries RPC", test_raft_append_entries() == 1);
    }
    
    /* 测试用例 6：选举机制 */
    {
        test_cond("Raft election", test_raft_election() == 1);
    }
    
    /* 测试用例 7：日志复制 */
    {
        test_cond("Raft log replication", test_raft_log_replication() == 1);
    }
    
    /* 测试用例 8：节点管理 */
    {
        test_cond("Raft peer management", test_raft_peer_management() == 1);
    }
    
    /* 测试用例 9：配置管理 */
    {
        test_cond("Raft configuration", test_raft_configuration() == 1);
    }
    
    /* 测试用例 10：时间驱动函数 */
    {
        test_cond("Raft tick function", test_raft_tick() == 1);
    }
    
    /* 生成测试报告 */
    /* 显示测试总数、通过数、失败数，如果有失败则退出程序 */
    test_report();
    return 1;  /* 所有测试通过 */
}

/**
 * @brief 主函数
 * 
 * 程序入口点，初始化必要的组件并运行所有测试。
 * 
 * @return 0 程序正常退出
 */
int main() {
    /* 初始化单调时钟（必须首先调用） */
    /* 单调时钟用于获取高精度时间戳，用于选举超时和心跳间隔 */
    const char *clock_type = monotonicInit();  /* 初始化并获取时钟类型 */
    (void)clock_type;  /* 抑制未使用变量警告 */
    
    /* 初始化随机数生成器 */
    /* 用于生成随机选举超时，避免多个节点同时发起选举 */
    srand(time(NULL));  /* 使用当前时间作为随机种子 */
    
    /* 运行所有测试 */
    test_api();  /* 执行所有测试用例并生成报告 */
    
    return 0;  /* 程序正常退出 */
}
