
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main() {
    int* result = malloc(10 * sizeof(int));
    for(int i = 0; i <  10; i++) {
        int value = 1877749 + i;
        result[i] = value;
    }
    for(int i = 0; i <  10; i++) {
        printf("atoi :%d\n", result[i]);
    }

    
}