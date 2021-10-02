//
// Created by 周国栋 on 2020/6/26.
//
#include <stdio.h>
//#include "libs/MathFunctions.h"
#include "./libs/sds.h"
#include "./libs/zmalloc.h"
#include "./libs/utils.h"
#include "./libs/dict.h"
#include "./libs/connection.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <float.h>
#include <stdint.h>
#include <errno.h>
#include <float.h>
#include "libs/syncio.h"
#include <sys/socket.h>
#include "libs/anet.h"
#include "libs/latteServer.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
//#include "csapp.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define VALUE_TYPE_INTEGER 0
#define VALUE_TYPE_FLOAT   1
#define VALUE_TYPE_DOUBLE     2
union tag_data {
    long long i;
    long double f;
    double d;
} tag_data;
typedef struct del_counter {
    unsigned long long gid:4;
    unsigned long long data_type:4;
    unsigned long long vcu: 56;
    union tag_data conv;
} del_counter;

del_counter* create_del_counter(long long gid, long long type, long long vcu) {
    del_counter* del = malloc(sizeof(del_counter));
    del->gid = gid;
    del->data_type = type;
    del->vcu = vcu;
    return del;
}

void free_del_counter(del_counter* del) {
    zfree(del);
}

int del_counter_to_string(del_counter* del, char* buf) {
    int len = 0;
    
    len += sprintf(buf + len, "%u:%lld:%u:",del->gid,  del->vcu, del->data_type);
    int s = 0;
    switch(del->data_type) {
        case VALUE_TYPE_INTEGER:
            len += sprintf(buf + len, "%lld", del->conv.i);
        break;
        case VALUE_TYPE_FLOAT:
            s = sizeof(del->conv.f);
            memcpy(buf + len, &(del->conv.f), s);
            len += s;
        break;
        case VALUE_TYPE_DOUBLE:
            s = sizeof(del->conv.d);
            memcpy(buf + len, &(del->conv.d), s);
            len += s;
        break;
    }
    return len;
}

typedef int (*GetDelCounterFunc)(void* data, int index, del_counter* value);

sds del_counters_to_sds(void* data, GetDelCounterFunc fun, int size) {
    char buf[100];
    int len = 0;
    for(int i = 0; i < size; i++) {
        del_counter del = {
            .gid = 0,
            .data_type = 0,
            .vcu = 0,
            .conv.d = 0,
        };
        if(!fun(data, i, &del)) return NULL;
        if(i > 0) {
            buf[len++] = ',';
        }
        len += del_counter_to_string(&del, buf + len);
    }
    return sdsnewlen(buf, len);
}

int sds_to_del_counter(sds data, del_counter** ds) {
    int len = 0;
    char* start = data;
    int offset = 0;
    char* split_index = NULL;
    del_counter* v = NULL;
    while((split_index = strstr(start + offset, ":")) != NULL) {
        long long gid = 0;
        if(!string2ll(start + offset, split_index - start - offset , &gid)) {
            goto error;
        }
        offset = split_index - start + 1;

        split_index = strstr(start + offset, ":");
        if(split_index == NULL) goto error;
        long long vcu = 0;
        if(!string2ll(start + offset , split_index - start - offset , &vcu)) {
            goto error;
        }
        offset = split_index - start + 1;

        split_index = strstr(start + offset, ":");
        if(split_index == NULL) goto error;
        long long type = 0;
        if(!string2ll(start + offset , split_index - start - offset, &type)) {
            goto error;
        }

        offset = split_index - start + 1;

        v = create_del_counter(gid, type, vcu);
        double d = 0;
        long long ll = 0;
        long double f = 0;
        int lsize = 0;
        switch (type)
        {
        case VALUE_TYPE_DOUBLE:
            d = *(double*)(start+offset);
            v->conv.d = d;
            offset += sizeof(double);
            break;
        case VALUE_TYPE_FLOAT:
            f = *(long double*)(start+offset);
            v->conv.f = f;
            offset += sizeof(long double);
            break;
        case VALUE_TYPE_INTEGER:
            split_index = strstr(start + offset, ",");
            if(split_index == NULL) {
                lsize = sdslen(data) - offset;
            } else {
                lsize = split_index - start - offset;
            }
            if(!string2ll(start + offset , lsize, &ll)) {
                goto error;
            }
            v->conv.i = ll;
            offset += lsize;
            break;
        default:
            goto error;
            break;
        }
        ds[len++] = v;
        v = NULL;
        if(start[offset] == ',') {
            offset++;
        }
    }
    return len;
error:
    if(v != NULL) free_del_counter(v);
    for(int i = 0; i < len; i++) {
        free_del_counter(ds[i]);
        ds[i] = NULL;
    }
    return 0;
}
int array_get_del_counter(void* data, int index, del_counter* value) {
    del_counter* array = (del_counter*)data;
    del_counter a = array[index];
    value->data_type = a.data_type;
    value->gid = a.gid;
    value->vcu = a.vcu;
    switch (value->data_type)
    {
    case VALUE_TYPE_INTEGER:
        value->conv.i = a.conv.i;
        break;
    case VALUE_TYPE_FLOAT:
        value->conv.f = a.conv.f;
        break;
    case VALUE_TYPE_DOUBLE:
        value->conv.d = a.conv.d;
        break;
    default:
        break;
    }
    return 1;
}

typedef int (*parseFunc)(char* buf, int* len, void* value);

int read_func(char* buf, int len , char* split_str, void* value, parseFunc func) {
    char* split_index = strstr(buf, split_str);
    int offset = 0;
    if(split_index == NULL) {
        offset = len;
    } else {
        offset = split_index - buf;
    }
    if(!func(buf, offset, value)) {
        return -1;
    }
    if(split_index != NULL) {
        offset += 1;
    }
    return offset;
}

void decToBin(int num)
{
    long long a = 1;
    char value[8] ;
    *value = *(char*)&a;
    for(int i = 0; i < 8;i++) {
        // printf("%s",char2bit(value[i]));
        decToBin(value[i]);
    }
    printf("\n");
	if(num>0)
		{
			decToBin(num/2);
			printf("%d",num%2);
		}
}


int test20() {
    sds v = sdsnewlen("xxx", 3);
    int len =  10000000;
    unsigned long long start_time = nstime();
    for(int i = 0; i < len;i++) {
        char buf[1000];
        int buf_len = 0;
        buf_len += ll2string(buf + buf_len, 21, 3);
        buf[buf_len++] = ':';
        buf_len += ll2string(buf + buf_len, 21, sdslen(v));
        buf[buf_len++] = ':';
        memcpy(buf + buf_len, v, sdslen(v));
        buf_len += sdslen(v);
        // long long ll = 0    ;
        // assert(!string2ll(v, sdslen(v), &ll));
        // long double ld = 0;
        // assert(!string2ld(v, sdslen(v), &ld)); 
    }
    printf("qps: %lld\n", (nstime() - start_time)/len);
    return len;
}
int malloc_test() {
    int len_8 = 1000000;
    void** malloc_8 = zmalloc(200*1024*1024*sizeof(void*));
    for(int i = 0; i < 1024*1024*200;i++) {
        malloc_8[i] = zmalloc(8);
    }
    unsigned long long start_time = nstime();
    for(int i = 0; i < len_8 ;i++) {
        void* a = zmalloc(8);
        zfree(a);
    }
    printf("qps: %lld\n", (nstime() - start_time)/len_8);
    for(int i = 0; i < 1024* 1024 *200;i++) {
        zfree(malloc_8[i]);
    }
    zfree(malloc_8);
    return 1;
}


uint64_t dictSdsHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, sdslen((char*)key));
}

int dictSdsKeyCompare(void *privdata, const void *key1,
                      const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);

    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

void dictSdsDestructor(void *privdata, void *val) {
    DICT_NOTUSED(privdata);
    sdsfree(val);
}

dictType hashDictType = {
    dictSdsHash,
    NULL,
    NULL,
    dictSdsKeyCompare,
    dictSdsDestructor,
    NULL
};

int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    return 1;
}

int main2() {
    latteServer* server = createServer(9999);
    printf("[main]%p..\n", server);
    if(runServer(server) == 0) {
        printf("run fail....\n");
        return 0;
    }
    if (aeCreateTimeEvent(server->el, 1, serverCron, NULL, NULL) == AE_ERR) {
        return 0;
    }
    aeMain(server->el);
    // aeEventLoop *el = aeCreateEventLoop(100);
    // connection* socket = connCreateSocket(el, NULL);
    // if (connConnect(socket, "127.0.0.1", 6379,
    //             NULL, connHandler) == C_ERR) {
    //     printf("conn error\n");
    //     connClose(socket);
    //     return C_ERR;
    // }
    // aeMain(el);

    // printf("2\n");
    // char* err = writeCommand(socket, 1, "CRDT.VC", NULL);
    // printf("3\n");
    // if(err) {
    //     printf("write error: %s\n", err);
    //     sdsfree(err);
    //     return C_ERR;
    // }
    // printf("???\n");
    // char* result = readCommand(socket, 1);
    // printf("read: %s\n", result);
    // sdsfree(result);
    
}

int sds2() {
    sds s = sdsnew("2:1.1,1:2:2:1.3");
    long type = 0;
    union tag_data a;
    long len = 0;
    del_counter b[3];
    int all_len = sdslen(s);
    long long ll = 0;
    int offset = 0;
    int o = read_func(s + offset, all_len - offset, ":", &ll, string2ll);
    if(o == -1) {
        printf("get ll error\n");
    }
    printf("%d %d\n", ll, o);
    offset += o;
    long double  ld = 0;
    o = read_func(s + offset, all_len - offset , ",", &ld, string2ld);
    if(o == -1) {
        printf("get ll error\n");
    }
    offset += o;
    printf("%.17Lf %d\n", ld, offset);


}
int test2() {
    long long a1  = 123123123123123123;
    long double b1 = 123123123123123123.0;
    long double c1 = *(long double*)&a1;
    long double d1 = (long double)a1;
    printf("%.17Lf %.17Lf\n", d1, c1);
    printf("%d %d\n", 1 | 2, 2|2);
    // long long a1 = LLONG_MAX>>4;
    // long long b1 = LLONG_MIN/16;
    // printf("%lld %lld\n", a1, b1);
    // del_counter a[3];
    // a[0].data_type = VALUE_TYPE_DOUBLE;
    // a[0].gid = 1;
    // a[0].vcu = 1000;
    // a[0].conv.d = 1.1;

    // a[1].data_type = VALUE_TYPE_INTEGER;
    // a[1].gid = 2;
    // a[1].vcu = 2000;
    // a[1].conv.i = 2;

    // a[2].data_type = VALUE_TYPE_FLOAT;
    // a[2].gid = 3;
    // a[2].vcu = 3000;
    // a[2].conv.f = 3.3;

    // sds data = del_counters_to_sds(a, array_get_del_counter, 3);
    // del_counter* ds[10];
    // int size = sds_to_del_counter(data, ds);
   
    // assert(size == 3);
    // assert(ds[0]->data_type == VALUE_TYPE_DOUBLE);
    // assert(ds[0]->gid == 1);
    // assert(ds[0]->vcu == 1000);
    // assert(ds[0]->conv.d == 1.1);

    // assert(ds[1]->data_type == VALUE_TYPE_INTEGER);
    // assert(ds[1]->gid == 2);
    // assert(ds[1]->vcu == 2000);
    // assert(ds[1]->conv.i == 2);

    // assert(ds[2]->data_type == VALUE_TYPE_FLOAT);
    // assert(ds[2]->gid == 3);
    // assert(ds[2]->vcu == 3000);
    // assert(ds[2]->conv.f == 3.3);

    long max = 1;
    long a = 9223372036854775807;
    
    printf("max long %ld\n", a);
	printf("sizeof(long ) = %d \n",sizeof(long)); //long类型数据占的字节数bytes
	printf("sizeof(int ) = %d \n",sizeof(int));   
	printf("sizeof(long long ) = %d \n",sizeof(long long));
	printf("Max of long is %ld \n",~(max<<(sizeof(long)*8-1)));//long 的最大值
	printf("Min of long is %ld \n",(max<<(sizeof(long)*8-1))); //long 的最小值
    return 1;
}

// int test1() {
//     del_counter* a[3];
//     del_counter* d = malloc(sizeof(del_counter));
//     d->data_type = VALUE_TYPE_DOUBLE;
//     d->gid = 1;
//     d->vcu = 1000;
//     d->conv.d = 1.1;
    
//     del_counter* d2 = malloc(sizeof(del_counter));
//     d2->data_type = VALUE_TYPE_INTEGER;
//     d2->gid = 2;
//     d2->vcu = 2000;
//     d2->conv.i = 2;

//     del_counter* d3 = malloc(sizeof(del_counter));
//     d3->data_type = VALUE_TYPE_FLOAT;
//     d3->gid = 3;
//     d3->vcu = 3000;
//     d3->conv.f = 3.3;
//     a[0] = d;
//     a[1] = d2;
//     a[2] = d3;
//     sds data = del_counters_to_sds(a, 3);

//     del_counter* ds[10];
//     int size = sds_to_del_counter(data, ds);
   
//     assert(size == 3);
//     assert(ds[0]->data_type == VALUE_TYPE_DOUBLE);
//     assert(ds[0]->gid == 1);
//     assert(ds[0]->vcu == 1000);
//     assert(ds[0]->conv.d == 1.1);

//     assert(ds[1]->data_type == VALUE_TYPE_INTEGER);
//     assert(ds[1]->gid == 2);
//     assert(ds[1]->vcu == 2000);
//     assert(ds[1]->conv.i == 2);

//     assert(ds[2]->data_type == VALUE_TYPE_FLOAT);
//     assert(ds[2]->gid == 3);
//     assert(ds[2]->vcu == 3000);
//     assert(ds[2]->conv.f == 3.3);
//     return 1;
// }


int main3() {
    int fd = open("./mmap.txt", O_RDWR, 0);  // O_RDWR 才能被读写
    if (fd < 0){
	    fprintf(stderr, "open: %s\n", strerror(errno));
        return -1;
    }
    lseek(fd, 1000, SEEK_SET);
    struct stat stat;
    fstat(fd, &stat);
    off_t size = stat.st_size;
	// mmapcopy(fd, stat.st_size);
    void* bufp;
    bufp = mmap(NULL,1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(bufp == (void*)-1) {
        fprintf(stderr, "mmap: %s\n", strerror(errno));
    }
    memcpy(bufp, "Linuxdd", 7);
    sleep(5);
    write(1, bufp, 7);
    munmap(bufp, 7);
}
typedef struct{
   char name[4];
   int age;
  }people;

int main4() {
    int fd,i;
   people *p_map;
   char temp;
   
   fd = open("./mmap.txt",O_CREAT|O_RDWR|O_TRUNC,00777);
   
   if(fd < 0)
   {
    printf("error open\n");
    exit(1);
   }
   
   lseek(fd,sizeof(people)*5-1,SEEK_SET);
   write(fd,"",1);
   p_map = (people*) mmap( NULL,sizeof(people)*10,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0 );
   close( fd );
   temp = 'a';
   for(i=0; i<10; i++)
   {
    temp += 1;
    memcpy( ( *(p_map+i) ).name, &temp,2 );
    ( *(p_map+i) ).age = 20+i;
   }
   
//    printf(" initialize over \n ");
//    sleep(10);
   munmap( p_map, sizeof(people)*10 );

}


struct IndexDirectory {
    sds indexPath;
} IndexDirectory;
typedef long DocId ;
struct VecPostings {
    DocId* postings;
    int length;
} VecPostings;
typedef struct VecPostingIterator {
    struct VecPostings* p;
    long index;
    long* entry;
    long* nextEntry;
} VecPostingIterator;
long VecPostingNext(struct VecPostingIterator *iter) {
    while(1) {
        if(iter->entry == NULL) {
            iter->index++;
            if (iter->index >= iter->p->length) break;
            iter->entry = &iter->p->postings[iter->index];
        } else {
            iter->entry = iter->nextEntry;
            iter->index++;
            if (iter->index >= iter->p->length) break;
        }
        if(iter->entry) {
            if(iter->index < iter->p->length) {
                iter->nextEntry = &iter->p->postings[iter->index+1];
                return iter->entry;
            } else {
                iter->nextEntry = -1;
            }
        }
    }
    return NULL;
}
VecPostingIterator* iter_post(struct VecPostings* p) {
    VecPostingIterator* iter = malloc(sizeof(*iter));
    iter->p = p;
    iter->index = -1;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    return iter;
}
struct IntersectionIterator {
    
} IntersectionIterator;
struct IntersectionIterator* iter_IntersectionIterator( ) {

}

int main6() {
    long size = 10;
    long* vec = malloc(sizeof(long) * size);
    for(long i = 0; i < size; i++) {
        vec[i] = i;
    }
    struct VecPostings* p = malloc(sizeof(VecPostings));
    p->length = size;
    p->postings = vec;
    VecPostingIterator* iter = iter_post(p);
    long* v = NULL;
    while((v=VecPostingNext(iter)) != NULL) {
        printf("%lld\n", *v);
    }
}


int minDistance(char * word1, char * word2){
    int w1 = strlen(word1);
    int w2 = strlen(word2);
    if(w1 < w2) {
        for(int i = 0; i < w1; i++) {
            str(word1[i]);
        }
        return w2 - w1;
    } else {
        return w1 - w2;
    }
}
int main() {
    int result = minDistance("horse", "ros");
    printf("result: %d \n", result);
    result = minDistance("ros", "horse");
    printf("result: %d \n", result);
}

