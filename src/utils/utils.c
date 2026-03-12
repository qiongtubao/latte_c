/**
 * @file utils.c
 * @brief 通用工具函数库，提供字符串转换、数字处理等实用功能
 */
#include "utils.h"
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>

/**
 * @brief 将字符串转换为long long整数
 * @param s 输入字符串
 * @param slen 字符串长度
 * @param value 输出的整数值指针
 * @return 成功返回1，失败返回0
 */
int string2ll(const char *s, size_t slen, long long *value) {
    const char *p = s;
    size_t plen = 0;
    int negative = 0;
    unsigned long long v;

    /* 零长度字符串不是有效数字 */
    if (plen == slen)
        return 0;

    /* 特殊情况：第一个也是唯一的数字是0 */
    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    /* 处理负数：只需设置一个标志并继续像处理正数一样。稍后转换为负数。 */
    if (p[0] == '-') {
        negative = 1;
        p++; plen++;

        /* 只有负号时中止 */
        if (plen == slen)
            return 0;
    }

    /* 第一个数字应该是1-9，否则字符串应该只是0 */
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0]-'0';
        p++; plen++;
    } else {
        return 0;
    }

    /* 解析所有其他数字，在每一步检查溢出 */
    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (ULLONG_MAX / 10)) /* 溢出 */
            return 0;
        v *= 10;

        if (v > (ULLONG_MAX - (p[0]-'0'))) /* 溢出 */
            return 0;
        v += p[0]-'0';

        p++; plen++;
    }

    /* 如果不是所有字节都被使用，则返回 */
    if (plen < slen)
        return 0;

    /* 如果需要转换为负数，并在从unsigned long long转换为long long时进行最终溢出检查 */
    if (negative) {
        if (v > ((unsigned long long)(-(LLONG_MIN+1))+1)) /* 溢出 */
            return 0;
        if (value != NULL) *value = -v;
    } else {
        if (v > LLONG_MAX) /* 溢出 */
            return 0;
        if (value != NULL) *value = v;
    }
    return 1;
}

/**
 * @brief 将SDS字符串转换为long long整数
 * @param value SDS字符串
 * @param result 输出的整数值指针
 * @return 成功返回1，失败返回0
 */
int sds2ll(sds_t value, long long * result) {
    return string2ll(value, sds_len(value), result);
}

/**
 * @brief 返回数字v在基数10下转换为字符串时的位数
 * @param v 输入的无符号64位整数
 * @return 返回位数
 */
uint32_t digits10(uint64_t v) {
    if (v < 10) return 1;
    if (v < 100) return 2;
    if (v < 1000) return 3;
    if (v < 1000000000000UL) {
        if (v < 100000000UL) {
            if (v < 1000000) {
                if (v < 10000) return 4;
                return 5 + (v >= 100000);
            }
            return 7 + (v >= 10000000UL);
        }
        if (v < 10000000000UL) {
            return 9 + (v >= 1000000000UL);
        }
        return 11 + (v >= 100000000000UL);
    }
    return 12 + digits10(v / 1000000000000UL);
}

/**
 * @brief 返回有符号64位整数在基数10下转换为字符串时的位数（包括符号）
 * @param v 输入的有符号64位整数
 * @return 返回位数
 */
uint32_t sdigits10(int64_t v) {
    if (v < 0) {
        uint64_t uv = (v != LLONG_MIN) ?
            (uint64_t) - v : ((uint64_t) LLONG_MAX) + 1;
        return digits10(uv) + 1;
    } else {
        return digits10(v);
    }
}

/**
 * @brief 将long long转换为字符串
 * @param dst 目标缓冲区
 * @param dstlen 缓冲区长度
 * @param svalue 要转换的long long值
 * @return 返回表示数字所需的字符数，如果缓冲区不够大则返回0
 */
int ll2string(char *dst, size_t dstlen, long long svalue) {
    static const char digits[201] =
        "0001020304050607080910111213141516171819"
        "2021222324252627282930313233343536373839"
        "4041424344454647484950515253545556575859"
        "6061626364656667686970717273747576777879"
        "8081828384858687888990919293949596979899";
    int negative;
    unsigned long long value;

    /* 主循环使用64位无符号整数以简化操作，所以我们在这里转换数字并记住它是否为负数 */
    if (svalue < 0) {
        if (svalue != LLONG_MIN) {
            value = -svalue;
        } else {
            value = ((unsigned long long) LLONG_MAX)+1;
        }
        negative = 1;
    } else {
        value = svalue;
        negative = 0;
    }

    /* 检查长度 */
    uint32_t const length = digits10(value)+negative;
    if (length >= dstlen) return 0;

    /* 空终止符 */
    uint32_t next = length;
    dst[next] = '\0';
    next--;
    while (value >= 100) {
        int const i = (value % 100) * 2;
        value /= 100;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
        next -= 2;
    }

    /* 处理最后1-2位数字 */
    if (value < 10) {
        dst[next] = '0' + (uint32_t) value;
    } else {
        int i = (uint32_t) value * 2;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
    }

    /* 添加符号 */
    if (negative) dst[0] = '-';
    return length;
}

/**
 * @brief 将long long转换为SDS字符串
 * @param ll 要转换的long long值
 * @return 返回包含数字字符串的SDS
 */
sds_t ll2sds(long long ll) {
    sds_t buffer = sds_new_len(NULL, MAX_ULL_CHARS);
    int len = ll2string(buffer, MAX_ULL_CHARS, ll);
    sds_set_len(buffer, len);
    return buffer;
}

/**
 * @brief 将字符串转换为double
 * @param s 输入字符串
 * @param slen 字符串长度
 * @param dp 输出的double值指针
 * @return 成功返回1，失败返回0
 */
int string2d(const char *s, size_t slen, double *dp) {
    errno = 0;
    char *eptr;
    *dp = strtod(s, &eptr);
    if (slen == 0 ||
        isspace(((const char*)s)[0]) ||
        (size_t)(eptr-(char*)s) != slen ||
        (errno == ERANGE &&
            (*dp == HUGE_VAL || *dp == -HUGE_VAL || *dp == 0)) ||
        isnan(*dp))
        return 0;
    return 1;
}

/**
 * @brief 将SDS字符串转换为double
 * @param v SDS字符串
 * @param dp 输出的double值指针
 * @return 成功返回1，失败返回0
 */
int sds2d(sds_t v, double *dp) {
    return string2d(v, sds_len(v), dp);
}

/**
 * @brief 将double转换为字符串表示
 * @param buf 目标缓冲区
 * @param len 缓冲区长度
 * @param value 要转换的double值
 * @return 返回所需的字节数
 */
int d2string(char *buf, size_t len, double value) {
    if (isnan(value)) {
        len = snprintf(buf,len,"nan");
    } else if (isinf(value)) {
        if (value < 0)
            len = snprintf(buf,len,"-inf");
        else
            len = snprintf(buf,len,"inf");
    } else if (value == 0) {
        /* 参见：http://en.wikipedia.org/wiki/Signed_zero，"比较"部分 */
        if (1.0/value < 0)
            len = snprintf(buf,len,"-0");
        else
            len = snprintf(buf,len,"0");
    } else {
#if (DBL_MANT_DIG >= 52) && (LLONG_MAX == 0x7fffffffffffffffLL)
        /* 检查浮点数是否在可以安全转换为long long的范围内。
         * 我们假设这里long long是64位的。
         * 我们也假设没有double精度<52位的实现。
         *
         * 在这些假设下，我们测试double是否在转换为long long安全的区间内。
         * 然后使用两次转换确保小数部分为零。如果所有这些都为真，
         * 我们使用速度更快的整数打印函数。 */
        double min = -4503599627370495; /* (2^52)-1 */
        double max = 4503599627370496; /* -(2^52) */
        if (value > min && value < max && value == ((double)((long long)value)))
            len = ll2string(buf,len,(long long)value);
        else
#endif
            len = snprintf(buf,len,"%.17g",value);
    }

    return len;
}

/**
 * @brief 将double转换为SDS字符串
 * @param d 要转换的double值
 * @return 返回包含数字字符串的SDS
 */
sds_t d2sds(double d) {
    char s[MAX_LONG_DOUBLE_CHARS];
    int size = d2string(s, MAX_LONG_DOUBLE_CHARS, d);
    return sds_new_len(s, size);
}

/**
 * @brief 将long double转换为字符串
 * @param buf 目标缓冲区
 * @param len 缓冲区长度
 * @param value 要转换的long double值
 * @param mode 转换模式（自动/十六进制/人类友好）
 * @return 返回字符串长度，如果缓冲区空间不够则返回0
 */
int ld2string(char *buf, size_t len, long double value, ld2string_mode mode) {
    size_t l = 0;

    if (isinf(value)) {
        /* 某些系统（如Solaris！）中的Libc会以不同方式格式化无穷大，
         * 所以最好以显式方式处理它 */
        if (len < 5) return 0; /* 空间不足。5是"-inf\0" */
        if (value > 0) {
            memcpy(buf,"inf",3);
            l = 3;
        } else {
            memcpy(buf,"-inf",4);
            l = 4;
        }
    } else {
        switch (mode) {
        case LD_STR_AUTO:
            l = snprintf(buf,len,"%.17Lg",value);
            if (l+1 > len) return 0; /* 空间不足 */
            break;
        case LD_STR_HEX:
            l = snprintf(buf,len,"%La",value);
            if (l+1 > len) return 0; /* 空间不足 */
            break;
        case LD_STR_HUMAN:
            /* 我们使用17位精度，因为对于128位浮点数，舍入后的精度
             * 能够以对用户"不令人惊讶"的方式表示大多数小数 */
            l = snprintf(buf,len,"%.17Lf",value);
            if (l+1 > len) return 0; /* 空间不足 */
            /* 现在删除'.'后的尾随零 */
            if (strchr(buf,'.') != NULL) {
                char *p = buf+l-1;
                while(*p == '0') {
                    p--;
                    l--;
                }
                if (*p == '.') l--;
            }
            break;
        default: return 0; /* 无效模式 */
        }
    }
    buf[l] = '\0';
    return l;
}

/**
 * @brief 将long double转换为SDS字符串
 * @param value 要转换的long double值
 * @param mode 转换模式
 * @return 返回包含数字字符串的SDS
 */
sds_t ld2sds(long double value, ld2string_mode mode) {
    sds_t buffer = sds_new_len("", MAX_LONG_DOUBLE_CHARS);
    int len = ld2string(buffer, MAX_LONG_DOUBLE_CHARS, value, mode);
    sds_set_len(buffer, len);
    return buffer;
}

/**
 * @brief 将字符串转换为long double
 * @param s 输入字符串
 * @param slen 字符串长度
 * @param dp 输出的long double值指针
 * @return 成功返回1，失败返回0
 */
int string2ld(const char *s, size_t slen, long double *dp) {
    char buf[MAX_LONG_DOUBLE_CHARS];
    long double value;
    char *eptr;

    if (slen == 0 || slen >= sizeof(buf)) return 0;
    memcpy(buf,s,slen);
    buf[slen] = '\0';

    errno = 0;
    value = strtold(buf, &eptr);
    if (isspace(buf[0]) || eptr[0] != '\0' ||
        (size_t)(eptr-buf) != slen ||
        (errno == ERANGE &&
            (value == HUGE_VAL || value == -HUGE_VAL || value == 0)) ||
        errno == EINVAL ||
        isnan(value))
        return 0;

    if (dp) *dp = value;
    return 1;
}

/**
 * @brief 将SDS字符串转换为long double
 * @param s SDS字符串
 * @param dp 输出的long double值指针
 * @return 成功返回1，失败返回0
 */
int sds2ld(sds_t s, long double *dp) {
    return string2ld(s, sds_len(s), dp);
}

/**
 * @brief 将字符串转换为unsigned long long值
 * @param s 输入字符串
 * @param value 输出的无符号长长整型值指针
 * @return 成功返回1，失败返回0
 */
int string2ull(const char *s, unsigned long long *value) {
    long long ll;
    if (string2ll(s,strlen(s),&ll)) {
        if (ll < 0) return 0; /* 负值超出范围 */
        *value = ll;
        return 1;
    }
    errno = 0;
    char *endptr = NULL;
    *value = strtoull(s,&endptr,10);
    if (errno == EINVAL || errno == ERANGE || !(*s != '\0' && *endptr == '\0'))
        return 0; /* strtoull()失败 */
    return 1; /* 转换完成！ */
}

/**
 * @brief 将unsigned long long转换为字符串
 * @param s 目标字符串缓冲区
 * @param len 缓冲区长度
 * @param v 要转换的无符号长长整型值
 * @return 返回字符串长度，失败返回0
 */
int ull2string(char* s, size_t len, unsigned long long v) {
    char *p, aux;
    size_t l;

    /* 生成字符串表示，此方法产生反向字符串 */
    p = s;
    do {
        *p++ = '0'+(v%10);
        v /= 10;
    } while(v);

    /* 计算长度并添加空终止符 */
    l = p-s;
    if (len < l) return 0;
    *p = '\0';

    /* 反转字符串 */
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/**
 * @brief 将unsigned long long转换为SDS字符串
 * @param ull 要转换的无符号长长整型值
 * @return 返回包含数字字符串的SDS
 */
sds_t ull2sds(unsigned long long ull) {
    sds_t buffer = sds_new_len("", MAX_ULL_CHARS);
    int len = ull2string(buffer, MAX_ULL_CHARS, ull);
    sds_set_len(buffer, len);
    return buffer;
}

/**
 * @brief 带有额外信息的断言函数
 * @param condition 断言条件
 * @param message 错误消息格式字符串
 * @param ... 可变参数
 */
void latte_assert_with_info(int condition, const char *message, ...) {
    if (!condition) {
        va_list args;
        va_start(args, message);
        fprintf(stderr, "Assertion failed: ");
        vfprintf(stderr, message, args);
        va_end(args);
        exit(1);
    }
}