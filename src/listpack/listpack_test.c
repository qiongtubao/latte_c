

#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "listpack.h"
#include <unistd.h>
#include <fcntl.h>
#include "log/log.h"
#include <strings.h>
#include <string.h>

/**
 * @brief 测试 listpack 数据结构的各种操作：创建、插入、追加、前置、替换、删除和迭代
 * @return int 测试通过返回 1
 */
int test_list_pack(void) {
    // 1. 创建和初始化
    list_pack_t* lp = list_pack_new(1024);
    assert(lp != NULL);
    lp = list_pack_shrink_to_fit(lp);
    unsigned char* newp = NULL;

    // 2. 在开头插入字符串 "hello"
    lp = list_pack_insert_string(lp, "hello", 5, lp + 6, LIST_PACK_BEFORE, &newp);
    assert(newp != NULL);
    assert(newp == lp + 6);
    // log_debug("test","%d\n",list_pack_bytes(lp));
    assert(list_pack_bytes(lp) == 14);

    // 3. 在第一个元素后插入整数 100
    lp = list_pack_insert_integer(lp, 100, lp + 6, LIST_PACK_AFTER, &newp);
    assert(newp != NULL);
    assert(newp == lp + 13);

    assert(list_pack_length(lp) == 2);
    assert(list_pack_bytes(lp) == 16);

    // 4. 追加整数和字符串
    lp = list_pack_append_integer(lp, 200);
    assert(list_pack_length(lp) == 3);
    assert(list_pack_bytes(lp) == 19);
    lp = list_pack_append_string(lp, "world", 5);
    assert(list_pack_length(lp) == 4);
    assert(list_pack_bytes(lp) == 26);

    // 5. 前置字符串和整数
    lp =list_pack_prepend_string(lp, "test", 4);
    assert(list_pack_length(lp) == 5);
    assert(list_pack_bytes(lp) == 32);

    lp = list_pack_prepend_integer(lp, 300);
    assert(list_pack_length(lp) == 6);
    assert(list_pack_bytes(lp) == 35);

    // 6. 替换操作
    unsigned char* tp = lp + 6; // 指向第一个元素
    lp = list_pack_replace_integer(lp, &tp, 400); // 替换为整数
    assert(list_pack_length(lp) == 6);
    assert(list_pack_bytes(lp) == 35);

    lp = list_pack_replace_string(lp, &tp, "test2", 5); // 再次替换为字符串
    assert(list_pack_length(lp) == 6);
    assert(list_pack_bytes(lp) == 39);

    // 7. 删除操作
    lp = list_pack_remove(lp, tp, &tp); // 删除第一个元素
    assert(list_pack_length(lp) == 5);
    assert(list_pack_bytes(lp) == 32);

    // 8. 范围删除
    tp = lp + 6; // 此时tp指向新的第一个元素
    lp = list_pack_remove_range_with_entry(lp, &tp, 4); // 删除前面4个元素，应该只剩最后一个 "world"
    assert(list_pack_length(lp) == 1);


    // 9. 迭代器测试，正向遍历并检查剩余的值
    latte_iterator_t* iter = list_pack_get_iterator(lp, LIST_PACK_AFTER);
    while (latte_iterator_has_next(iter)) {
        latte_list_pack_value_t* value = latte_iterator_next(iter);
        // if (value->sval != NULL) {
        //     log_debug("test","str: %s, len: %d",value->sval, value->slen);
        // } else {
        //     log_debug("test","int: %lld",value->lval);
        // }
        assert(value->slen == 5);
        assert(strncmp(value->sval, "world", 5) == 0);
    }
    latte_iterator_delete(iter);

    // 10. 清理资源
    list_pack_delete(lp);
    return 1;
}

/**
 * @brief 执行所有的 listpack 模块测试
 * @return int 成功返回 1
 */
int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        log_module_init();
        assert(log_add_stdout("test", LOG_DEBUG) == 1);
        assert(log_add_file("test", "./test.log", LOG_INFO) == 1);
        test_cond("test_list_pack test",
            test_list_pack() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}