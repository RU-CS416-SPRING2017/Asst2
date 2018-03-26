#include "my_pthread_t.h"

void * test(void * nun) {
    char * some = malloc(4096 * 800);
    sprintf(some, "in test");
    printf("%s\n", some);
    free(some);
    malloc(40);
    sprintf(some, "in test1");
    printf("%s\n", some);
    return some;
}

void * test2(void * nun) {
    char * some = malloc(4096 * 800);
    sprintf(some, "in test2");
    printf("%s\n", some);
    free(some);
    return NULL;
}

int main() {

    pthread_t t, t2;
    pthread_create(&t, NULL, test, NULL);
    pthread_create(&t2, NULL, test2, NULL);
    printf("in main\n");
    void * ret;
    pthread_join(t, &ret);
    char * some = &ret;
    printf("out %s\n", some);
    pthread_join(t2, &ret);
    some = &ret;
    printf("out %s\n", some);
    return 0;
}
