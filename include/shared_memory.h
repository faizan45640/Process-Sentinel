#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SHM_KEY 0x54321
#define SEM_KEY 0x67890
#define MAX_PROC 10

typedef struct {
    int pid;
    char name[32];
    float cpu;
    long ram;
    char status[16];    // "STABLE", "ANOMALY", "PAUSED"
    char ai_advice[64]; // Last instruction from AI
    int active;         // 1 if being monitored, 0 if empty slot
} ProcessInfo;

typedef struct {
    int total_monitored;
    ProcessInfo procs[MAX_PROC];
} SentinelBoard;

// Function Prototypes
int get_shm_id();
SentinelBoard* attach_shm(int shmid);
void detach_shm(SentinelBoard* board);
int get_sem_id();
void sem_lock(int semid);
void sem_unlock(int semid);

void update_dashboard(int pid, const char* name, float cpu, long ram, char* status, char* advice);

#endif