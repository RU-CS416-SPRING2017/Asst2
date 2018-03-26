#include "my_pthread_t.h"

void * test(void * nun) {
    char * some = malloc(4096 * 800);
    sprintf(some, "in test");
    // char * some = "in some";
    printf("%s\n", some);
    return NULL;
}

void * test2(void * nun) {
    char * some = malloc(4096 * 800);
    sprintf(some, "in test2");
    // char * some = "in some";
    printf("%s\n", some);
    return NULL;
}

int main() {

    pthread_t t, t2;
    pthread_create(&t, NULL, test, NULL);
    pthread_create(&t2, NULL, test2, NULL);
    printf("in main\n");
    void * ret;
    pthread_join(t, &ret);
    pthread_join(t2, &ret);
    return 0;
}
