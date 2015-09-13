#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

void* t_func(void* index) {
    printf("Running thread #%d...\n", *(int*) index);
    return NULL;
}

int main(void) {
    int i;
    int t_indices[10];
    pthread_t threads[10];

    for (i = 0; i < 10; ++i) {
        t_indices[i] = i + 1;
        pthread_create(&threads[i], NULL, t_func, (void*) &t_indices[i]);
    }

    for (i = 0; i < 10; ++i) {
        void* ret;
        pthread_join(threads[i], (void**) &ret);
    }

    puts("Done.");

    return 0;
}
