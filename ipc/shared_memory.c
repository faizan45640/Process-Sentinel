#include "shared_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>

int get_shm_id() {
    return shmget(SHM_KEY, sizeof(SentinelBoard), IPC_CREAT | 0666);
}

SentinelBoard* attach_shm(int shmid) {
    return (SentinelBoard*)shmat(shmid, NULL, 0);
}

void detach_shm(SentinelBoard* board) {
    shmdt(board);
}

int get_sem_id() {
    int semid = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (semid != -1) {
        // Newly created, initialize to 1
        if (semctl(semid, 0, SETVAL, 1) == -1) {
            perror("semctl SETVAL failed");
        }
    } else {
        // Already exists, just get it
        semid = semget(SEM_KEY, 1, 0666);
    }
    return semid;
}

void reset_sem(int semid) {
    // Force the semaphore to 1 (unlocked)
    if (semctl(semid, 0, SETVAL, 1) == -1) {
        perror("[IPC] Warning: Failed to reset semaphore");
    } else {
        printf("[IPC] Semaphore reset to unlocked state.\n");
    }
}

void sem_lock(int semid) {
    struct sembuf sb = {0, -1, 0}; // Decrease by 1 (Wait)
    semop(semid, &sb, 1);
}

void sem_unlock(int semid) {
    struct sembuf sb = {0, 1, 0};  // Increase by 1 (Signal)
    semop(semid, &sb, 1);
}

void force_cleanup_shm() {
    // 1. Clean Shared Memory
    int shmid = shmget(SHM_KEY, 0, 0666);
    if (shmid != -1) {
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("[IPC] Warning: Failed to remove stale SHM");
        } else {
            printf("[IPC] Stale SHM removed (ID: %d).\n", shmid);
        }
    }

    // 2. Clean Semaphore
    int semid = semget(SEM_KEY, 1, 0666);
    if (semid != -1) {
        if (semctl(semid, 0, IPC_RMID) == -1) {
            perror("[IPC] Warning: Failed to remove stale Semaphore");
        } else {
            printf("[IPC] Stale Semaphore removed (ID: %d).\n", semid);
        }
    }
}

void update_dashboard(int pid, const char* name, float cpu, long ram, char* status, char* advice) {
    int shmid = get_shm_id();
    if (shmid == -1) {
        perror("[IPC] Error: get_shm_id failed in update_dashboard");
        return;
    }
    
    SentinelBoard* board = attach_shm(shmid);
    if (board == (void*)-1) {
        perror("[IPC] Error: attach_shm failed in update_dashboard");
        return;
    }

    int semid = get_sem_id();
    sem_lock(semid);

    // 1. Search for existing entry
    int found_idx = -1;
    int empty_idx = -1;

    for (int i = 0; i < MAX_PROC; i++) {
        if (board->procs[i].active && board->procs[i].pid == pid) {
            found_idx = i;
            break;
        }
        if (!board->procs[i].active && empty_idx == -1) {
            empty_idx = i;
        }
    }

    int target_idx = (found_idx != -1) ? found_idx : empty_idx;

    if (target_idx != -1) {
        board->procs[target_idx].pid = pid;
        board->procs[target_idx].active = 1;
        board->procs[target_idx].cpu = cpu;
        board->procs[target_idx].ram = ram;
        
        // Update name if provided and not just "pid_XXX" generic unless that's all we have
        if (name && strlen(name) > 0) {
             strncpy(board->procs[target_idx].name, name, sizeof(board->procs[target_idx].name) - 1);
             board->procs[target_idx].name[sizeof(board->procs[target_idx].name) - 1] = '\0';
        }

        strncpy(board->procs[target_idx].status, status, sizeof(board->procs[target_idx].status) - 1);
        board->procs[target_idx].status[sizeof(board->procs[target_idx].status) - 1] = '\0';
        
        strncpy(board->procs[target_idx].ai_advice, advice, sizeof(board->procs[target_idx].ai_advice) - 1);
        board->procs[target_idx].ai_advice[sizeof(board->procs[target_idx].ai_advice) - 1] = '\0';

        // Update History (Shift Right)
        for (int j = HISTORY_LEN - 1; j > 0; j--) {
            board->procs[target_idx].cpu_history[j] = board->procs[target_idx].cpu_history[j-1];
            board->procs[target_idx].ram_history[j] = board->procs[target_idx].ram_history[j-1];
        }
        board->procs[target_idx].cpu_history[0] = cpu;
        board->procs[target_idx].ram_history[0] = ram;
    }

    sem_unlock(semid);
    detach_shm(board);
}

void remove_terminated_from_dashboard() {
    int shmid = get_shm_id();
    if (shmid == -1) return;
    
    SentinelBoard* board = attach_shm(shmid);
    if (board == (void*)-1) return;

    int semid = get_sem_id();
    sem_lock(semid);

    for (int i = 0; i < MAX_PROC; i++) {
        if (board->procs[i].active && strcmp(board->procs[i].status, "TERMINATED") == 0) {
            board->procs[i].active = 0; // Free the slot
            printf("[IPC] Removed TERMINATED entry for PID %d\n", board->procs[i].pid);
        }
    }

    sem_unlock(semid);
    detach_shm(board);
}