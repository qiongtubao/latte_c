
#include "mmap.h"

#if defined(MMAP_TEST_MAIN)
#include <stdio.h>
#include "testhelp.h"
 #include <sys/mman.h> 
#include <sys/types.h> 
#include <fcntl.h> 
#include <unistd.h> 

typedef struct{ 
   char name[6]; 
   int age; 
}people; 
int test_mmap() {
    int fd,i; 
    people *p_map; 
    char temp[10] = "hello"; 
   
    fd = open("hello.txt",O_CREAT|O_RDWR,0666); 
    int r = ftruncate(fd, sizeof(people)*10);
    if(fd < 0) {
        printf("error open\n");
        exit(1);
    }
    // lseek(fd,sizeof(long long)*5-1,SEEK_SET); 
    p_map = (long long*) mmap( NULL,sizeof(people)*10,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0 );
    // close( fd ); 
    if(p_map == NULL){ 
        printf("p_map base mmap is error\n"); 
        close( fd );
        exit(1);
    }
    for(i=0; i<10; i++) { 
        // temp += 1; 
        memcpy( ( *(p_map+i) ).name, temp,strlen(temp) + 1 ); 
        ( *(p_map+i) ).age = 20+i; 
    } 
    // printf(" initialize over \n ");
    // sleep(10); 
    munmap( p_map, sizeof(long long)*10 ); 
    close( fd );
    // printf( "umap ok \n" );
    return 1;
}

int test1_mmap() {
    int fd,i; 
    people *p_map; 
    fd=open( "hello.txt",O_CREAT|O_RDWR,0666 ); 
    p_map = (people*)mmap(NULL,sizeof(people)*10,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    for(i = 0;i<10;i++) 
    { 
        printf( "name: %s age %d;\n",(*(p_map+i)).name, (*(p_map+i)).age ); 
    } 
    munmap( p_map,sizeof(people)*10 );
}

int main() {
    int childpid;
    if((childpid = fork()) == 0) {
        /** Child **/
        test1_mmap();
    } else {
        /** Parent */ 
        test_mmap();
    }
}
#endif
