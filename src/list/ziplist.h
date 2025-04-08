

#ifndef __LATTE_ZIP_LIST_H
#define __LATTE_ZIP_LIST_H

typedef struct {
    unsigned char *sval;
    unsigned int slen;
    long long lval;
} zip_list_entry_t;

unsigned char *zip_list_new(void);
//不释放*zl 内存 只是删除数据
unsigned char *zip_list_delete(unsigned char *zl, unsigned char **p);


#endif