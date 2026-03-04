odb 模块已实现并通过测试，摘要如下。
1. 头文件 odb.h
oio 创建/释放：odb_oio_create_buffer()、odb_oio_create_file(FILE *fp)、odb_oio_free(o)、odb_oio_buffer_rewind(o)（仅 buffer 后端）
字符串：odb_write_string(o, s, len)（4 字节长度 + 数据）、odb_read_string(o) 返回 sds，调用方 sds_delete
二进制：odb_write_binary(o, buf, len)、odb_read_binary(o, &out, &out_len)，读出的内存由调用方 zfree(out)
数字（小端）：odb_write_u8/u16/u32/u64、odb_read_u8/u16/u32/u64
2. 实现 odb.c
Buffer 后端：用 sds 做缓冲，read/write/tell/flush 实现，创建时 sds_empty()
File 后端：对 FILE* 做 fread/fwrite/ftello/fflush，不拥有 fp
长度用 uint32 小端；数字为定长小端；字符串/二进制均为「4 字节长度 + 数据」
使用项目内 sds 接口：sds_len、sds_cat_len、sds_empty、sds_new_len、sds_delete
3. 单测 odb_test.c
oio buffer 创建
字符串写/回绕/读
二进制写/回绕/读
数字 u8/u16/u32/u64 写/回绕/读
混合：string + u32 + binary + u8 一轮往返
4. Makefile
增加 ae 到 LIB_MODULES，并先 include ae 再 connection，保证链接到 ae_file_event_new、ae_wait 等（若之前 connection 未重编，可能曾出现未定义符号，重编 connection 后已通过）。