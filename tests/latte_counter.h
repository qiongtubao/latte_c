#include <stdio.h>
#include <stdlib.h>
typedef struct crdt_tag {
    unsigned long long data_type: 1; //0 value 1 tombstone
    unsigned long long type:3; //1 base 2  addcounter 4  delcounter 
    unsigned long long gid: 4;
    unsigned long long ops: 56;
} __attribute__ ((packed, aligned(1))) crdt_tag;


typedef struct element {
    unsigned long long len: 4;
    unsigned long long tags: 60;
} __attribute__ ((packed, aligned(1))) element;

typedef struct element2 {
    unsigned long long len: 8;
    unsigned long long len: 4;
    unsigned long long tags: 52;
} __attribute__ ((packed, aligned(1))) element2; 
