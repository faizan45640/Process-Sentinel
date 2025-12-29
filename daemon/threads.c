#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

// Include your project headers
#include "monitor.h"
#include "anomaly.h"
#include "shared_memory.h"
#include "threads.h"

// Forward declarations of functions from other files
extern void ask_ai_for_help(int pid, const char* name, float cpu, float mean, float stddev, const char* trigger, char* result_buffer);
extern int execute_ai_action(int pid, const char* ai_response);
extern void update_dashboard(int pid, const char* name, float cpu, long ram, char* status, char* advice);

void log_anomaly(int pid, const char* name, float cpu, const char* reason) {
    FILE *f = fopen("sentinel_anomalies.log", "a");
    if (f) {
        time_t now = time(NULL);
        char *t_str = ctime(&now);
        t_str[strlen(t_str)-1] = '\0'; // Remove newline
        fprintf(f, "[%s] ANOMALY: %s (PID %d) - CPU: %.2f%% - Reason: %s\n", t_str, name, pid, cpu, reason);
        fclose(f);
    }
}

void mark_process_terminated(int pid, const char* reason) {
    update_dashboard(pid, "", 0, 0, "TERMINATED", (char*)reason);
}

void* monitor_thread(void* arg) {
    MonitoredThread* t_args = (MonitoredThread*)arg;
    int pid = t_args->pid;
    char proc_name[32];
    strncpy(proc_name, t_args->name, 32);
    
    // 1. Initialize History for this specific PID
    ProcessHistory history;
    init_history(&history, pid);

    ProcessStats stats;
    char ai_instruction[256] = "N/A";
    char current_status[16] = "STABLE";

    printf("[THREAD %d] Monitoring started for %s\n", pid, proc_name);

    while (t_args->keep_running) {
        // 2. Fetch real-time stats (from monitor.c)
        if (get_process_stats(pid, &stats) != 0) {
            printf("[THREAD %d] Process terminated or lost.\n", pid);
            // If we have a pending AI instruction (e.g. we just killed it), keep it.
            // Otherwise, assume natural exit.
            if (strcmp(ai_instruction, "N/A") != 0 && strncmp(ai_instruction, "Monitoring", 10) != 0) {
                 // Keep the last AI advice as the reason
                mark_process_terminated(pid, ai_instruction);
            } else {
                mark_process_terminated(pid, "Process exited naturally.");
            }
            break; // Exit thread
        }

        // 3. Update Statistical History (from anomaly.c)
        update_history(&history, stats.cpu_usage, stats.rss);

        // 4. Check for Anomalies (Hard Rules OR Z-Score)
        char anomaly_reason[64];
        if (check_anomaly(&history, stats.cpu_usage, proc_name, anomaly_reason)) {
            strcpy(current_status, "ANOMALY");
            printf("[THREAD %d] ! %s\n", pid, anomaly_reason);
            
            // Log it
            log_anomaly(pid, proc_name, stats.cpu_usage, anomaly_reason);

            // 5. Connect to AI Bridge (from ai_agent.py via sockets)
            char ai_result[1024]; // Larger buffer for raw response
            ask_ai_for_help(pid, proc_name, stats.cpu_usage, 
                            history.cpu_mean, history.cpu_stddev, anomaly_reason, ai_result);
            
            // Extract the "MSG" part for the dashboard
            char *msg_ptr = strstr(ai_result, "MSG:");
            if (msg_ptr) {
                msg_ptr += 4; // Skip "MSG:"
                while (*msg_ptr == ' ' || *msg_ptr == '[') msg_ptr++; // Skip format chars
                
                strncpy(ai_instruction, msg_ptr, 255);
                ai_instruction[255] = '\0';
                
                // Clean trailing bracket if present
                char *end = strrchr(ai_instruction, ']');
                if (end) *end = '\0';
            } else {
                strncpy(ai_instruction, ai_result, 255);
            }

            // 6. Execute the Action (from recovery.c)
            int new_pid = execute_ai_action(pid, ai_result);
            
            // Handle RESTART (PID Change)
            if (new_pid > 0) {
                 printf("[THREAD %d] Switching monitor to new PID %d (Restarted)\n", pid, new_pid);
                 
                 // Mark old process as terminated with the specific reason
                 mark_process_terminated(pid, ai_instruction);

                 // Update local variables
                 pid = new_pid;
                 t_args->pid = new_pid; // Update shared struct so Main knows too
                 
                 // Reset history for new process
                 init_history(&history, pid);
                 
                 // Reset AI instruction for the NEW process
                 strcpy(ai_instruction, "Monitoring started (Restarted)");
                 
                 // Continue loop immediately to monitor new PID
                 continue;
            }
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