#include <stdio.h>
#include "mylib.h"

int main() {

    char * some = malloc(8);

    sprintf(some, "whats up");

    printf("%s\n", some);

    return 0;
}
