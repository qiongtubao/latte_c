/**
 * @file endianconv.c
 * @brief 字节序转换工具函数，支持16位、32位、64位整数的大小端转换
 */

#include "endianconv.h"
#include <stdint.h>

/* Toggle the 16 bit unsigned integer pointed by *p from little endian to
 * big endian */
/*  将指针 *p 指向的16位无符号整数从小端序转换为大端序 */
/**
 * @brief 将16位整数进行字节序转换(小端序⇔大端序)
 * @param p 指向16位整数的指针
 */
void memrev16(void *p) {
    unsigned char *x = p, t;

    t = x[0];    // 交换字节0和字节1
    x[0] = x[1];
    x[1] = t;
}

/* Toggle the 32 bit unsigned integer pointed by *p from little endian to
 * big endian */
/* 将指针 *p 指向的32位无符号整数从小端序转换为大端序 */
/**
 * @brief 将32位整数进行字节序转换(小端序⇔大端序)
 * @param p 指向32位整数的指针
 */
void memrev32(void *p) {
    unsigned char *x = p, t;

    t = x[0];    // 交换字节0和字节3
    x[0] = x[3];
    x[3] = t;
    t = x[1];    // 交换字节1和字节2
    x[1] = x[2];
    x[2] = t;
}
/* Toggle the 64 bit unsigned integer pointed by *p from little endian to
 * big endian */
/*将指针 *p 指向的64位无符号整数从小端序转换为大端序*/
/**
 * @brief 将64位整数进行字节序转换(小端序⇔大端序)
 * @param p 指向64位整数的指针
 */
void memrev64(void *p) {
    unsigned char *x = p, t;

    t = x[0];    // 交换字节0和字节7
    x[0] = x[7];
    x[7] = t;
    t = x[1];    // 交换字节1和字节6
    x[1] = x[6];
    x[6] = t;
    t = x[2];    // 交换字节2和字节5
    x[2] = x[5];
    x[5] = t;
    t = x[3];    // 交换字节3和字节4
    x[3] = x[4];
    x[4] = t;
}

/**
 * @brief 将16位整数值进行字节序转换并返回
 * @param v 16位整数值
 * @return 转换后的16位整数值
 */
uint16_t intrev16(uint16_t v) {
    memrev16(&v);
    return v;
}

/**
 * @brief 将32位整数值进行字节序转换并返回
 * @param v 32位整数值
 * @return 转换后的32位整数值
 */
uint32_t intrev32(uint32_t v) {
    memrev32(&v);
    return v;
}

/**
 * @brief 将64位整数值进行字节序转换并返回
 * @param v 64位整数值
 * @return 转换后的64位整数值
 */
uint64_t intrev64(uint64_t v) {
    memrev64(&v);
    return v;
}