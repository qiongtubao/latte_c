#include "options.h"
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

static struct option longOpts[] = {
    {"daemon", no_argument, NULL, 'D'},
    {"add", required_argument, 0, 'a'}
};


/**
 * test
 */ 
int main(int argc, char* argv[]) {
    int c;
    while(1) {
        int this_option_optind = optind? optind: 1;
        int option_index = 0;
        c = getopt_long(argc, argv, "abc:d:012", longOpts, &option_index);
        if(c == -1) break;
        switch(c) {
            case 'a':
                printf("a: %s\n", optarg);
            break;
            case 'D':
                printf("D: %s\n", optarg);
            break;
            default:
                printf("????\n");
                break;

        }
    }
    return 0;
}