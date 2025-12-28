#ifndef ANOMALY_H
#define ANOMALY_H

#include "monitor.h"  // Assuming ProcessStats is in here if needed elsewhere

#define HISTORY_SIZE 20
#define Z_THRESHOLD 3.0f     // Use float suffix for clarity

typedef enum {
    STATE_STABLE,
    STATE_WARNING,
    STATE_ANOMALY
} ProcessState;

typedef struct {
    int pid;
    float cpu_history[HISTORY_SIZE];
    long ram_history[HISTORY_SIZE];
    int head;           // Points to next write position (circular buffer)
    int count;          // How many samples we have (0 to HISTORY_SIZE)
    float cpu_mean;     // Running mean (updated incrementally)
    float cpu_stddev;   // Running standard deviation
    float ram_mean;     // Optional: also track RAM mean if you want
    ProcessState state;
} ProcessHistory;

/* Function prototypes */
void init_history(ProcessHistory *h, int pid);
void update_history(ProcessHistory *h, float new_cpu, long new_ram);
int check_anomaly(ProcessHistory *h, float current_cpu);  // Returns 1 if anomaly
void update_state(ProcessHistory *h);                    // New: manage state transitions

#endif // ANOMALY_H