#include "mylib.h"
#include <stdio.h>

int main() {

    char * string1 = malloc(80);
    char * string2 = malloc(80);
    int * number = malloc(sizeof(int));

    sprintf(string1, "this is string 1");
    sprintf(string2, "this is string 2");
    *number = 3;

    printf("string 1: %s\nstring 2: %s\nnumber: %d\n", string1, string2, *number);

    return 0;
}
