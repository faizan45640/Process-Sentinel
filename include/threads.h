#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <signal.h>

// Thread argument structure
typedef struct {
    pthread_t tid;
    int pid;
    char name[32];
    volatile sig_atomic_t keep_running;
    int active;
} MonitoredThread;

void* monitor_thread(void* arg);

#endif // THREADS_H
