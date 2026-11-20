#include <pthread.h>
#include <stdint.h>
int64_t work(int32_t n);
static void *run(void *n) { work((int32_t)(intptr_t)n); return 0; }
void run_threads(int32_t count) {
    for (int32_t i = 0; i < count; i++) {
        pthread_t t;
        pthread_create(&t, 0, run, (void *)(intptr_t)i);
        pthread_join(t, 0);
    }
}
