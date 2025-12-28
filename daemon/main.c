#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "mq.h"
#include "shared_memory.h"
#include "threads.h"

#if defined(_WIN32) || defined(_WIN64)
#error "This project is designed for Linux and is not compatible with Windows."
#endif

#define MAX_THREADS MAX_PROC // Max concurrent monitored processes

// --- Global Thread Pool ---
MonitoredThread thread_pool[MAX_THREADS];

void initialize_thread_pool() {
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_pool[i].active = 0;
        thread_pool[i].pid = -1;
    }
}

void add_process_to_monitor(int pid, const char* name) {
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
        if (!thread_pool[i].active) break;
    }

    if (i == MAX_THREADS) {
        fprintf(stderr, "[MAIN] Error: Thread pool is full. Cannot monitor new process.\n");
        return;
    }

    thread_pool[i].pid = pid;
    strncpy(thread_pool[i].name, name, sizeof(thread_pool[i].name) - 1);
    thread_pool[i].keep_running = 1;
    thread_pool[i].active = 1;

    if (pthread_create(&thread_pool[i].tid, NULL, monitor_thread, &thread_pool[i]) != 0) {
        perror("[MAIN] Failed to create thread");
        thread_pool[i].active = 0; // Rollback
    } else {
        printf("[MAIN] Successfully started monitoring PID %d in thread slot %d.\n", pid, i);
    }
}

void remove_process_from_monitor(int pid) {
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
        if (thread_pool[i].active && thread_pool[i].pid == pid) break;
    }

    if (i == MAX_THREADS) {
        fprintf(stderr, "[MAIN] Error: PID %d not found in monitor list.\n", pid);
        return;
    }

    printf("[MAIN] Stopping monitor for PID %d...\n", pid);
    thread_pool[i].keep_running = 0; // Signal thread to stop
    
    // Wait for the thread to finish
    if (pthread_join(thread_pool[i].tid, NULL) != 0) {
        perror("[MAIN] Failed to join thread");
    }

    thread_pool[i].active = 0; // Free up the slot
    thread_pool[i].pid = -1;
    printf("[MAIN] Monitor for PID %d stopped and slot %d freed.\n", pid, i);
}

void shutdown_all_monitors() {
    printf("[MAIN] Shutting down all monitoring threads...\n");
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_pool[i].active) {
            remove_process_from_monitor(thread_pool[i].pid);
        }
    }
}

int main() {
    printf("Starting Process Sentinel Daemon...\n");

    initialize_thread_pool();

    int mqid = init_mq();
    if (mqid == -1) exit(1);

    int shmid = get_shm_id();
    int semid = get_sem_id();
    SentinelBoard* board = attach_shm(shmid);
    
    sem_lock(semid);
    board->total_monitored = 0;
    for(int i=0; i<MAX_PROC; i++) board->procs[i].active = 0;
    sem_unlock(semid);

    printf("Daemon is online. Waiting for CLI commands...\n");

    SentinelMsg msg;
    int keep_daemon_running = 1;
    while (keep_daemon_running) {
        if (receive_sentinel_msg(mqid, &msg) == 0) {
            switch(msg.cmd) {
                case CMD_ADD:
                    printf("[MAIN] Command: ADD PID %d (%s)\n", msg.pid, msg.proc_name);
                    add_process_to_monitor(msg.pid, msg.proc_name);
                    break;
                case CMD_REMOVE:
                    printf("[MAIN] Command: REMOVE PID %d\n", msg.pid);
                    remove_process_from_monitor(msg.pid);
                    break;
                case CMD_SHUTDOWN:
                    printf("[MAIN] Command: SHUTDOWN\n");
                    keep_daemon_running = 0; // Exit loop
                    break;
                default:
                    fprintf(stderr, "[MAIN] Unknown command received: %ld\n", msg.cmd);
            }
        }
    }

    shutdown_all_monitors();

    // Cleanup
    detach_shm(board);
    cleanup_mq(mqid);
    printf("Process Sentinel Daemon has shut down gracefully.\n");
    return 0;
}