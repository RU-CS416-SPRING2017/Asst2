#include "my_pthread_t.h"

void * test(void * nun) {
    char * some = malloc(100);
    sprintf(some, "in test");
    // char * some = "in some";
    printf("%s\n", some);
    return NULL;
}

int main() {

    my_pthread_t t;
    pthread_create(&t, NULL, test, NULL);
    printf("in main\n");
    void * ret;
    pthread_join(t, &ret);
    return 0;
}
