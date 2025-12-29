#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "mq.h"
#include "shared_memory.h"
#include "threads.h"
#include "anomaly.h"

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
    int empty_slot = -1;
    int reused_slot = -1;

    // Search for existing slot with same name (reuse logic) or empty slot
    for (i = 0; i < MAX_THREADS; i++) {
        if (thread_pool[i].active && strncmp(thread_pool[i].name, name, 32) == 0) {
            // Found a process with same name. Is it dead?
            if (kill(thread_pool[i].pid, 0) == -1 && errno == ESRCH) {
                 reused_slot = i;
                 break;
            }
        }
        if (!thread_pool[i].active && empty_slot == -1) {
            empty_slot = i;
        }
    }

    // Decide which slot to use
    int target_idx = (reused_slot != -1) ? reused_slot : empty_slot;

    if (target_idx == -1) {
        fprintf(stderr, "[MAIN] Error: Thread pool is full. Cannot monitor new process.\n");
        return;
    }

    // If reusing, clean up old thread first if it's still technically 'active' but dead
    if (reused_slot != -1) {
        pthread_join(thread_pool[target_idx].tid, NULL);
        printf("[MAIN] Reusing slot %d for restarted process '%s' (Old PID: %d -> New PID: %d)\n", 
               target_idx, name, thread_pool[target_idx].pid, pid);
    }

    thread_pool[target_idx].pid = pid;
    strncpy(thread_pool[target_idx].name, name, sizeof(thread_pool[target_idx].name) - 1);
    thread_pool[target_idx].keep_running = 1;
    thread_pool[target_idx].active = 1;

    if (pthread_create(&thread_pool[target_idx].tid, NULL, monitor_thread, &thread_pool[target_idx]) != 0) {
        perror("[MAIN] Failed to create thread");
        thread_pool[target_idx].active = 0; // Rollback
    } else {
        printf("[MAIN] Successfully started monitoring PID %d in thread slot %d.\n", pid, target_idx);
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

void cleanup_terminated_processes() {
    printf("[MAIN] Cleaning up terminated processes from dashboard...\n");
    // 1. Remove from internal thread pool
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_pool[i].active) {
            if (kill(thread_pool[i].pid, 0) == -1 && errno == ESRCH) {
                // Process is dead
                remove_process_from_monitor(thread_pool[i].pid);
            }
        }
    }
    // 2. Remove "TERMINATED" entries from Shared Memory
    remove_terminated_from_dashboard();
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
    // Disable stdout buffering to ensure logs are captured immediately
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Starting Process Sentinel Daemon...\n");

    // Pre-emptive cleanup of stale resources
    force_cleanup_shm();
    force_cleanup_mq();

    initialize_thread_pool();
    load_rules();

    int mqid = init_mq();
    if (mqid == -1) {
        fprintf(stderr, "[MAIN] Error: Failed to initialize Message Queue.\n");
        exit(1);
    }

    int shmid = get_shm_id();
    if (shmid == -1) {
        perror("[MAIN] Error: get_shm_id failed");
        exit(1);
    }

    int semid = get_sem_id();
    if (semid == -1) {
        perror("[MAIN] Error: get_sem_id failed");
        exit(1);
    }
    
    // Safety: Reset semaphore to ensure we don't hang on a stale lock
    reset_sem(semid);

    SentinelBoard* board = attach_shm(shmid);
    if (board == (void*)-1) {
        perror("[MAIN] Error: attach_shm failed");
        exit(1);
    }
    
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
                case CMD_CLEAN:
                    printf("[MAIN] Command: CLEAN (Drop terminated)\n");
                    cleanup_terminated_processes();
                    break;
                case CMD_SHUTDOWN:
                    printf("[MAIN] Command: SHUTDOWN\n");
                    keep_daemon_running = 0; // Exit loop
                    break;
                default:
                    fprintf(stderr, "[MAIN] Unknown command received: %d\n", msg.cmd);
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