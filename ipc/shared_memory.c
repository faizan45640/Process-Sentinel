#include "shared_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    int semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    // Initialize semaphore to 1 (unlocked) if newly created
    // Note: This is a bit race-prone if multiple processes init, but okay for this lab.
    // Ideally we'd use semctl with IPC_EXCL or check if it's already initialized.
    // For now we assume the Daemon initializes it first in main.c
    return semid;
}

void sem_lock(int semid) {
    struct sembuf sb = {0, -1, 0}; // Decrease by 1 (Wait)
    semop(semid, &sb, 1);
}

void sem_unlock(int semid) {
    struct sembuf sb = {0, 1, 0};  // Increase by 1 (Signal)
    semop(semid, &sb, 1);
}

void update_dashboard(int pid, const char* name, float cpu, long ram, char* status, char* advice) {
    int shmid = get_shm_id();
    if (shmid == -1) return;
    
    SentinelBoard* board = attach_shm(shmid);
    if (board == (void*)-1) return;

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
    }

    sem_unlock(semid);
    detach_shm(board);
}