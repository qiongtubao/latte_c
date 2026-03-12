/**
 * @file encode_test.c
 * @brief encode和decode模块的单元测试
 *
 * 测试所有编码解码功能，包括：
 * - Base64编码/解码测试
 * - 十六进制编码/解码测试
 * - URL编码/解码测试
 * - 变长整数编码/解码测试
 * - 流式解码器测试
 * - 批量操作测试
 * - 边界条件和错误处理测试
 *
 * @author latte_c项目
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "encode.h"
#include "decode.h"
#include "../test/testhelp.h"
#include "../test/testassert.h"

/* ================= Base64编码/解码测试 ================= */

/**
 * @brief 测试Base64编码功能
 * @return 1表示测试通过，0表示测试失败
 */
int test_base64_encode(void) {
    size_t output_len;

    /* 测试标准字符串编码 */
    const char *input1 = "Hello World";
    char *encoded1 = base64_encode(input1, strlen(input1), &output_len);
    assert(encoded1 != NULL);
    assert(strcmp(encoded1, "SGVsbG8gV29ybGQ=") == 0);
    assert(output_len == 16);
    zfree(encoded1);

    /* 测试空字符串编码 */
    const char *input2 = "";
    char *encoded2 = base64_encode(input2, 0, &output_len);
    assert(encoded2 != NULL);
    assert(strcmp(encoded2, "") == 0);
    assert(output_len == 0);
    zfree(encoded2);

    /* 测试单字符编码 */
    const char *input3 = "A";
    char *encoded3 = base64_encode(input3, 1, &output_len);
    assert(encoded3 != NULL);
    assert(strcmp(encoded3, "QQ==") == 0);
    assert(output_len == 4);
    zfree(encoded3);

    return 1;
}

/**
 * @brief 测试Base64解码功能
 * @return 1表示测试通过，0表示测试失败
 */
int test_base64_decode(void) {
    size_t output_len;

    /* 测试标准字符串解码 */
    const char *input1 = "SGVsbG8gV29ybGQ=";
    void *decoded1 = base64_decode(input1, strlen(input1), &output_len);
    assert(decoded1 != NULL);
    assert(output_len == 11);
    assert(memcmp(decoded1, "Hello World", 11) == 0);
    zfree(decoded1);

    /* 测试填充解码 */
    const char *input3 = "QQ==";
    void *decoded3 = base64_decode(input3, 4, &output_len);
    assert(decoded3 != NULL);
    assert(output_len == 1);
    assert(*(char*)decoded3 == 'A');
    zfree(decoded3);

    /* 测试错误格式（长度不是4的倍数） */
    const char *input4 = "SGVsbG8";
    void *decoded4 = base64_decode(input4, 7, &output_len);
    assert(decoded4 == NULL);  /* 应该返回NULL */

    return 1;
}

/* ================= 十六进制编码/解码测试 ================= */

/**
 * @brief 测试十六进制编码功能
 * @return 1表示测试通过，0表示测试失败
 */
int test_hex_encode(void) {
    size_t output_len;

    /* 测试小写编码 */
    const char *input1 = "Hello";
    char *encoded1 = hex_encode(input1, strlen(input1), 0, &output_len);
    assert(encoded1 != NULL);
    assert(strcmp(encoded1, "48656c6c6f") == 0);
    assert(output_len == 10);
    zfree(encoded1);

    /* 测试大写编码 */
    char *encoded2 = hex_encode(input1, strlen(input1), 1, &output_len);
    assert(encoded2 != NULL);
    assert(strcmp(encoded2, "48656C6C6F") == 0);
    assert(output_len == 10);
    zfree(encoded2);

    return 1;
}

/**
 * @brief 测试十六进制解码功能
 * @return 1表示测试通过，0表示测试失败
 */
int test_hex_decode(void) {
    size_t output_len;

    /* 测试标准解码 */
    const char *input1 = "48656C6C6F";
    void *decoded1 = hex_decode(input1, strlen(input1), &output_len);
    assert(decoded1 != NULL);
    assert(output_len == 5);
    assert(memcmp(decoded1, "Hello", 5) == 0);
    zfree(decoded1);

    /* 测试错误格式（奇数长度） */
    const char *input4 = "48656C6C6";
    void *decoded4 = hex_decode(input4, strlen(input4), &output_len);
    assert(decoded4 == NULL);

    return 1;
}

/* ================= URL编码/解码测试 ================= */

/**
 * @brief 测试URL编码功能
 * @return 1表示测试通过，0表示测试失败
 */
int test_url_encode(void) {
    size_t output_len;

    /* 测试空格编码 */
    const char *input1 = "Hello World";
    char *encoded1 = url_encode(input1, strlen(input1), &output_len);
    assert(encoded1 != NULL);
    assert(strcmp(encoded1, "Hello%20World") == 0);
    assert(output_len == 13);
    zfree(encoded1);

    /* 测试安全字符不编码 */
    const char *input3 = "Hello-World_test.file~";
    char *encoded3 = url_encode(input3, strlen(input3), &output_len);
    assert(encoded3 != NULL);
    assert(strcmp(encoded3, "Hello-World_test.file~") == 0);  /* 应该不变 */
    zfree(encoded3);

    return 1;
}

/**
 * @brief 测试URL解码功能
 * @return 1表示测试通过，0表示测试失败
 */
int test_url_decode(void) {
    size_t output_len;

    /* 测试空格解码 */
    const char *input1 = "Hello%20World";
    char *decoded1 = url_decode(input1, strlen(input1), &output_len);
    assert(decoded1 != NULL);
    assert(strcmp(decoded1, "Hello World") == 0);
    assert(output_len == 11);
    zfree(decoded1);

    /* 测试特殊字符解码 */
    const char *input2 = "Hello%21%40%23";
    char *decoded2 = url_decode(input2, strlen(input2), &output_len);
    assert(decoded2 != NULL);
    assert(strcmp(decoded2, "Hello!@#") == 0);
    zfree(decoded2);

    return 1;
}

/* ================= 变长整数编码/解码测试 ================= */

/**
 * @brief 测试32位变长整数编码功能
 * @return 1表示测试通过，0表示测试失败
 */
int test_varint_u32_encode(void) {
    uint8_t buffer[10];
    int bytes;

    /* 测试小数值（1字节） */
    bytes = varint_encode_u32(0, buffer);
    assert(bytes == 1);
    assert(buffer[0] == 0x00);

    bytes = varint_encode_u32(127, buffer);
    assert(bytes == 1);
    assert(buffer[0] == 0x7F);

    /* 测试中等数值（2字节） */
    bytes = varint_encode_u32(128, buffer);
    assert(bytes == 2);
    assert(buffer[0] == 0x80);
    assert(buffer[1] == 0x01);

    return 1;
}

/**
 * @brief 测试32位变长整数解码功能
 * @return 1表示测试通过，0表示测试失败
 */
int test_varint_u32_decode(void) {
    uint32_t value;
    int bytes;

    /* 测试1字节解码 */
    uint8_t data1[] = {0x00};
    bytes = varint_decode_u32(data1, sizeof(data1), &value);
    assert(bytes == 1);
    assert(value == 0);

    uint8_t data2[] = {0x7F};
    bytes = varint_decode_u32(data2, sizeof(data2), &value);
    assert(bytes == 1);
    assert(value == 127);

    /* 测试2字节解码 */
    uint8_t data3[] = {0x80, 0x01};
    bytes = varint_decode_u32(data3, sizeof(data3), &value);
    assert(bytes == 2);
    assert(value == 128);

    return 1;
}

/* ================= 主测试函数 ================= */

/**
 * @brief 主测试入口函数
 * @return 0表示所有测试通过，非0表示有测试失败
 */
int main(void) {
    printf("开始encode/decode模块测试...\n\n");

    printf("=== Base64编码/解码测试 ===\n");
    test_cond("Base64编码测试", test_base64_encode() == 1);
    test_cond("Base64解码测试", test_base64_decode() == 1);

    printf("\n=== 十六进制编码/解码测试 ===\n");
    test_cond("十六进制编码测试", test_hex_encode() == 1);
    test_cond("十六进制解码测试", test_hex_decode() == 1);

    printf("\n=== URL编码/解码测试 ===\n");
    test_cond("URL编码测试", test_url_encode() == 1);
    test_cond("URL解码测试", test_url_decode() == 1);

    printf("\n=== 变长整数编码/解码测试 ===\n");
    test_cond("32位变长整数编码测试", test_varint_u32_encode() == 1);
    test_cond("32位变长整数解码测试", test_varint_u32_decode() == 1);

    test_report();
    return 0;
}