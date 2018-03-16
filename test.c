#include "my_pthread_t.h"

void * test(void * nun) {
    printf("in test\n");
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
