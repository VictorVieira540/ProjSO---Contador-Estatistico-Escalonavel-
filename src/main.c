#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define DEFAULT_THREADS 8
#define DEFAULT_INCREMENTS 10000000
#define DEFAULT_THRESHOLD 1000

typedef struct {
    long long local;
} local_counter_t;

long long global_counter = 0;
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

local_counter_t *counters;
int num_threads;
int increments_per_thread;
int threshold;

void flush_local(int id) {
    pthread_mutex_lock(&global_lock);
    global_counter += counters[id].local;
    pthread_mutex_unlock(&global_lock);
    counters[id].local = 0;
}

void *worker(void *arg) {
    int id = (int)(long)arg;

    for (int i = 0; i < increments_per_thread; i++) {
        counters[id].local++;

        if (counters[id].local >= threshold)
            flush_local(id);
    }

    // Flush final para garantir que o resto seja somado
    flush_local(id);
    return NULL;
}

int main(int argc, char *argv[]) {
    num_threads = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS;
    increments_per_thread = (argc > 2) ? atoi(argv[2]) : DEFAULT_INCREMENTS;
    threshold = (argc > 3) ? atoi(argv[3]) : DEFAULT_THRESHOLD;

    printf("Iniciando contador escalonável:\n");
    printf("- Threads: %d\n", num_threads);
    printf("- Incrementos/thread: %d\n", increments_per_thread);
    printf("- Threshold: %d\n\n", threshold);

    pthread_t threads[num_threads];
    counters = malloc(sizeof(local_counter_t) * num_threads);

    for (int i = 0; i < num_threads; i++)
        counters[i].local = 0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_threads; i++)
        pthread_create(&threads[i], NULL, worker, (void *)(long)i);

    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Contagem final: %lld\n", global_counter);
    printf("Tempo decorrido: %.6f segundos\n", elapsed);

    free(counters);
    return 0;
}
