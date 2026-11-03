#include <pthread.h>
#include <stdint.h>
int on_thread(int);
static void *run(void *n) { return (void *)(intptr_t)on_thread((int)(intptr_t)n); }
int from_thread(int n) {
    pthread_t t;
    void *got;
    pthread_create(&t, 0, run, (void *)(intptr_t)n);
    pthread_join(t, &got);
    return (int)(intptr_t)got;
}
