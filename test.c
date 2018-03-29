#include "my_pthread_t.h"

void * test(void * nun) {
    char * some = shalloc(40);
    sprintf(some, "in test");
    printf("%s\n", some);
	fflush(stdout);
    free(some);
    char * sometwo = shalloc(40);
    sprintf(sometwo, "in test1");
    printf("%s\n", sometwo);
    pthread_exit(sometwo);
}

void * test2(void * nun) {
    char * some = shalloc(40);
    sprintf(some, "in test2");
    printf("%s\n", some);
    // free(some);
    pthread_exit(some);
}

int main() {
    char * rand = malloc(200);
    sprintf(rand, "thanks dog");
    printf("%s\n", rand);
    fflush(stdout);
    pthread_t t, t2;
    pthread_create(&t, NULL, test, NULL);
    pthread_create(&t2, NULL, test2, NULL);
    printf("in main\n");
    void * ret;
	char * some;
    pthread_join(t, &ret);
	fflush(stdout);
	printf("back in main\n");
	fflush(stdout);
	some = ret;
	printf("%s\n", some);
    pthread_join(t2, &ret);
	some = ret;
	printf("%s\n", some);
    return 0;
}
