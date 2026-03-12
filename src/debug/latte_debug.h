/**
 * @file latte_debug.h
 * @brief 调试断言与崩溃接口
 *        提供断言失败处理函数和 panic 崩溃函数，支持在运行时检测编程错误。
 *        - latte_assert：断言宏，条件为假时打印位置信息并终止程序
 *        - latte_panic：格式化错误信息并立即终止程序
 *        - latte_unreachable：标记不可达代码分支，辅助编译器优化并防止误执行
 */
#ifndef __LATTE_DEBUG_H
#define __LATTE_DEBUG_H



/**
 * @brief 断言失败时的底层处理函数
 *        打印断言表达式字符串、所在文件路径及行号，然后终止程序。
 *        通常不直接调用，请使用 latte_assert 宏。
 * @param estr 失败的断言表达式字符串（由宏自动传入 #_e）
 * @param file 源文件路径（由宏自动传入 __FILE__）
 * @param line 源文件行号（由宏自动传入 __LINE__）
 */
void _latte_assert(const char *estr, const char *file, int line);

#ifdef __GNUC__
/**
 * @brief panic 底层函数（GCC 版，支持 printf 格式检查）
 *        格式化输出错误信息后立即终止程序，不可恢复。
 *        通常不直接调用，请使用 latte_panic 宏。
 * @param file 源文件路径（由宏自动传入 __FILE__）
 * @param line 源文件行号（由宏自动传入 __LINE__）
 * @param msg  printf 格式化字符串
 * @param ...  可变参数，与 msg 中的格式占位符对应
 */
void _latte_panic(const char *file, int line, const char *msg, ...)
    __attribute__ ((format (printf, 3, 4)));
#else
/**
 * @brief panic 底层函数（非 GCC 版）
 *        格式化输出错误信息后立即终止程序，不可恢复。
 * @param file 源文件路径
 * @param line 源文件行号
 * @param msg  printf 格式化字符串
 * @param ...  可变参数
 */
void _latte_panic(const char *file, int line, const char *msg, ...);
#endif

/**
 * @brief 不可达代码标记宏
 *        GCC 4+ 展开为 __builtin_unreachable()，告知编译器该分支不可到达，
 *        辅助优化并消除"控制流到达函数尾部"警告。
 *        其他编译器退化为 abort()，确保不可达时立即终止。
 */
#if __GNUC__ >= 4
#define latte_unreachable __builtin_unreachable
#else
#define latte_unreachable abort
#endif

/**
 * @brief 断言宏
 *        若表达式 _e 为假（0），调用 _latte_assert 打印位置信息并触发
 *        latte_unreachable()（通知编译器此路径不可达，优化分支）。
 * @param _e 断言表达式；为真时无操作，为假时终止程序
 */
#define latte_assert(_e) ((_e)?(void)0 : (_latte_assert(#_e,__FILE__,__LINE__),latte_unreachable()))

/**
 * @brief panic 宏
 *        格式化输出错误信息后立即终止程序；
 *        调用 latte_unreachable() 通知编译器该行之后代码不可达。
 * @param ... printf 格式化字符串及可变参数
 */
#define latte_panic(...) _latte_panic(__FILE__,__LINE__,__VA_ARGS__),latte_unreachable()


#endif
