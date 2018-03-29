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
	//return sometwo;
}

void * test2(void * nun) {
    char * some = shalloc(40);
    sprintf(some, "in test2");
    printf("%s\n", some);
    // free(some);
	pthread_exit(some);
    //return some;
}

int main() {
	//char * rand = malloc(40);
    //sprintf(rand, "in main malloc");
    //printf("%s\n", rand);
	//char * randtwo = shalloc(40);
    //sprintf(randtwo, "in main shalloc");
    //printf("%s\n", randtwo);
	
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
