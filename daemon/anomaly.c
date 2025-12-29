#include "anomaly.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_RULES 20
Rule rules[MAX_RULES];
int rule_count = 0;

void load_rules() {
    FILE *f = fopen("rules.conf", "r");
    if (!f) return;
    
    rule_count = 0;
    char line[128];
    while (fgets(line, sizeof(line), f) && rule_count < MAX_RULES) {
        char name[64];
        int limit;
        if (sscanf(line, "%63s %d", name, &limit) == 2) {
            strncpy(rules[rule_count].process_name, name, 63);
            rules[rule_count].cpu_limit = limit;
            rule_count++;
        }
    }
    fclose(f);
    printf("[ANOMALY] Loaded %d rules from rules.conf\n", rule_count);
}

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

int check_anomaly(ProcessHistory *h, float current_cpu, const char *name, char *reason_out) {
    // 1. Check Hard Rules (Priority)
    // We reload rules occasionally? No, for now assume static or main calls reload.
    // Ideally, we might check file mod time, but keep it simple.
    
    for (int i = 0; i < rule_count; i++) {
        if (strstr(name, rules[i].process_name) != NULL) { // Partial match allowed (e.g. "chrome")
            if (current_cpu > rules[i].cpu_limit) {
                h->state = STATE_ANOMALY;
                sprintf(reason_out, "RULE_BREACH (Limit: %d%%)", rules[i].cpu_limit);
                return 1;
            }
        }
    }

    if (h->count < 5) return STATE_STABLE; // Need minimum data points

    // The Z-Score Formula: (x - mean) / stddev
    // Fix: If stddev is too low (process was stable), use a minimum value (0.5) to avoid div/0
    // This ensures that jumping from 0% to 100% is still caught as an anomaly.
    float safe_stddev = (h->cpu_stddev > 0.5) ? h->cpu_stddev : 0.5;
    
    float z_score = fabs(current_cpu - h->cpu_mean) / safe_stddev;

    if (z_score > Z_THRESHOLD) {
        h->state = STATE_ANOMALY;
        sprintf(reason_out, "STATISTICAL_ANOMALY (Z: %.2f)", z_score);
        return 1; // Anomaly detected!
    }
    
    h->state = STATE_STABLE;
    return 0;
}