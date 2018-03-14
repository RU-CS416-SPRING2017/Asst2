#include "mylib.h"
#include <stdio.h>

int main() {

    char * string1 = malloc(100);
    sprintf(string1, "this is string 1");
    printf("string 1: %s\n", string1);

    char * string2 = malloc(100);
    sprintf(string2, "this is string 2");
    printf("string 2: %s\n", string2);

    int * number = malloc(sizeof(int));
    *number = 3;
    printf("number: %d\n", *number);

    free(string1);

    char * string3 = malloc(100);
    sprintf(string3, "this is string 3");
    printf("string 3: %s\n", string3);


    return 0;
}
