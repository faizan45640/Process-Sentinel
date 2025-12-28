#include "anomaly.h"
#include <math.h>
#include <stdio.h>

void init_history(ProcessHistory *h, int pid) {
    h->pid = pid;
    h->head = 0;
    h->count = 0;
    h->cpu_mean = 0;
    h->cpu_stddev = 0;
    h->state = STATE_STABLE;
}

void calculate_stats(ProcessHistory *h) {
    if (h->count == 0) return;

    // 1. Calculate Mean
    float sum = 0;
    for (int i = 0; i < h->count; i++) sum += h->cpu_history[i];
    h->cpu_mean = sum / h->count;

    // 2. Calculate Standard Deviation
    float sum_sq_diff = 0;
    for (int i = 0; i < h->count; i++) {
        sum_sq_diff += pow(h->cpu_history[i] - h->cpu_mean, 2);
    }
    h->cpu_stddev = sqrt(sum_sq_diff / h->count);
}

void update_history(ProcessHistory *h, float new_cpu, long new_ram) {
    h->cpu_history[h->head] = new_cpu;
    h->ram_history[h->head] = new_ram;
    
    h->head = (h->head + 1) % HISTORY_SIZE;
    if (h->count < HISTORY_SIZE) h->count++;

    calculate_stats(h);
}

int check_anomaly(ProcessHistory *h, float current_cpu) {
    if (h->count < 5) return STATE_STABLE; // Need minimum data points

    // The Z-Score Formula: (x - mean) / stddev
    // Avoid division by zero if stddev is 0
    float z_score = (h->cpu_stddev > 0.1) ? 
                    fabs(current_cpu - h->cpu_mean) / h->cpu_stddev : 0;

    if (z_score > Z_THRESHOLD) {
        h->state = STATE_ANOMALY;
        return 1; // Anomaly detected!
    }
    
    h->state = STATE_STABLE;
    return 0;
}