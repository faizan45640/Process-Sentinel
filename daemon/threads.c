#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

// Include your project headers
#include "monitor.h"
#include "anomaly.h"
#include "shared_memory.h"
#include "threads.h"

// Forward declarations of functions from other files
extern void ask_ai_for_help(int pid, const char* name, float cpu, float mean, float stddev, char* result_buffer);
extern void execute_ai_action(int pid, const char* ai_response);
extern void update_dashboard(int pid, const char* name, float cpu, long ram, char* status, char* advice);

void* monitor_thread(void* arg) {
    MonitoredThread* t_args = (MonitoredThread*)arg;
    int pid = t_args->pid;
    char proc_name[32];
    strncpy(proc_name, t_args->name, 32);
    
    // 1. Initialize History for this specific PID
    ProcessHistory history;
    init_history(&history, pid);

    ProcessStats stats;
    char ai_instruction[128] = "N/A";
    char current_status[16] = "STABLE";

    printf("[THREAD %d] Monitoring started for %s\n", pid, proc_name);

    while (t_args->keep_running) {
        // 2. Fetch real-time stats (from monitor.c)
        if (get_process_stats(pid, &stats) != 0) {
            printf("[THREAD %d] Process terminated or lost. Cleaning up dashboard.\n", pid);
            update_dashboard(pid, proc_name, 0, 0, "TERMINATED", "Process no longer exists.");
            break; // Exit thread
        }

        // 3. Update Statistical History (from anomaly.c)
        update_history(&history, stats.cpu_usage, stats.rss);

        // 4. Check for Anomalies using Z-Score
        if (check_anomaly(&history, stats.cpu_usage)) {
            strcpy(current_status, "ANOMALY");
            printf("[THREAD %d] ! ANOMALY DETECTED (CPU: %.2f%%)\n", pid, stats.cpu_usage);

            // 5. Connect to AI Bridge (from ai_agent.py via sockets)
            char ai_result[256];
            ask_ai_for_help(pid, proc_name, stats.cpu_usage, 
                            history.cpu_mean, history.cpu_stddev, ai_result);
            
            strncpy(ai_instruction, ai_result, 128);

            // 6. Execute the Action (from recovery.c)
            execute_ai_action(pid, ai_result);
        } else {
            strcpy(current_status, "STABLE");
        }

        // 7. Update the Shared Memory Dashboard (for sentinel_cli)
        update_dashboard(pid, proc_name, stats.cpu_usage, stats.rss, current_status, ai_instruction);

        // Interval: Sleep to avoid the Sentinel consuming too much CPU
        sleep(2); 
    }

    printf("[THREAD %d] Monitoring stopped for %s.\n", pid, proc_name);
    pthread_exit(NULL);
}