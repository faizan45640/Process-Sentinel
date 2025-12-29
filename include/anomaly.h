#ifndef ANOMALY_H
#define ANOMALY_H

#include "monitor.h"

#define HISTORY_SIZE 10
#define Z_THRESHOLD 2.0

#define STATE_STABLE 0
#define STATE_ANOMALY 1

typedef struct {
    int pid;
    float cpu_history[HISTORY_SIZE];
    long ram_history[HISTORY_SIZE];
    int head;
    int count;
    float cpu_mean;
    float cpu_stddev;
    int state;
} ProcessHistory;

typedef struct {
    char process_name[64];
    int cpu_limit;
} Rule;

// Functions
void init_history(ProcessHistory *h, int pid);
void update_history(ProcessHistory *h, float new_cpu, long new_ram);

// Updated to return the reason if anomaly is found
int check_anomaly(ProcessHistory *h, float current_cpu, const char *name, char *reason_out);

void load_rules();

#endif