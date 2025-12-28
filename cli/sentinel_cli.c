#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include "shared_memory.h"
#include "mq.h"

// ANSI Color Codes
#define RESET  "\x1b[0m"
#define BOLD   "\x1b[1m"
#define RED    "\x1b[31m"
#define GREEN  "\x1b[32m"
#define YELLOW "\x1b[33m"
#define CYAN   "\x1b[36m"
#define CLEAR  "\e[1;1H\e[2J"

// Function to find PID by process name
int find_pid_by_name(const char* target_name) {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            char path[256], name[256];
            snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
            
            FILE* f = fopen(path, "r");
            if (f) {
                if (fgets(name, sizeof(name), f)) {
                    name[strcspn(name, "\n")] = 0;
                    if (strcasecmp(name, target_name) == 0) {
                        int pid = atoi(entry->d_name);
                        fclose(f);
                        closedir(dir);
                        return pid;
                    }
                }
                fclose(f);
            }
        }
    }
    closedir(dir);
    return -1;
}

void print_usage() {
    printf(BOLD "Usage:" RESET "\n");
    printf("  ./sentinel_cli --monitor\n");
    printf("      " "Open the real-time monitoring dashboard.\n\n");
    printf("  ./sentinel_cli --add " BOLD "<pid>" RESET "\n");
    printf("      " "Start monitoring a process by its PID.\n\n");
    printf("  ./sentinel_cli --add-by-name " BOLD "<name>" RESET "\n");
    printf("      " "Start monitoring a process by its name (e.g., 'firefox').\n\n");
    printf("  ./sentinel_cli --remove " BOLD "<pid>" RESET "\n");
    printf("      " "Stop monitoring a process.\n\n");
    printf("  ./sentinel_cli --shutdown\n");
    printf("      " "Signal the Sentinel daemon to shut down gracefully.\n");
}

void show_dashboard() {
    int shmid = get_shm_id();
    int semid = get_sem_id();
    SentinelBoard* board = attach_shm(shmid);

    while (1) {
        printf(CLEAR);
        printf(BOLD CYAN "====================================================================\n");
        printf("                PROCESS SENTINEL - AI-POWERED MONITOR               \n");
        printf("====================================================================\n" RESET);
        printf("%-8s %-15s %-10s %-10s %-15s\n", "PID", "NAME", "CPU%", "RAM(KB)", "STATUS");
        printf("--------------------------------------------------------------------\n");

        sem_lock(semid);
        for (int i = 0; i < MAX_PROC; i++) {
            if (board->procs[i].active) {
                char *color = (strcmp(board->procs[i].status, "ANOMALY") == 0) ? RED : GREEN;
                if (strcmp(board->procs[i].status, "PAUSED") == 0) color = YELLOW;

                printf("%-8d %-15s %s%-10.2f %-10ld %-15s" RESET "\n", 
                       board->procs[i].pid, board->procs[i].name, color,
                       board->procs[i].cpu, board->procs[i].ram, board->procs[i].status);
                
                if (strlen(board->procs[i].ai_advice) > 0 && strcmp(board->procs[i].ai_advice, "N/A") != 0) {
                    printf(BOLD "   └─ AI Advice: " RESET "%s\n", board->procs[i].ai_advice);
                }
            }
        }
        sem_unlock(semid);

        printf("====================================================================\n");
        printf("Refresh: 1s | Ctrl+C to Exit Dashboard\n");
        sleep(1);
    }
    detach_shm(board);
}

// Safely parse a string to a PID
int parse_pid(const char* str) {
    char* end;
    long pid = strtol(str, &end, 10);
    if (end == str || *end != '\0' || pid <= 0) {
        fprintf(stderr, RED "Error: Invalid PID '%s'. PID must be a positive number.\n" RESET, str);
        return -1;
    }
    return (int)pid;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    int mqid = init_mq();
    SentinelMsg msg;

    if (strcmp(argv[1], "--add") == 0 && argc == 3) {
        msg.pid = parse_pid(argv[2]);
        if (msg.pid == -1) return 1;
        msg.cmd = CMD_ADD;
        snprintf(msg.proc_name, sizeof(msg.proc_name), "pid_%d", msg.pid); // Generic name
        send_sentinel_msg(mqid, &msg);
        printf(GREEN "✔ Command sent: Start monitoring PID %d\n" RESET, msg.pid);

    } else if (strcmp(argv[1], "--add-by-name") == 0 && argc == 3) {
        msg.pid = find_pid_by_name(argv[2]);
        if (msg.pid == -1) {
            fprintf(stderr, RED "Error: Process '%s' not found.\n" RESET, argv[2]);
            return 1;
        }
        msg.cmd = CMD_ADD;
        snprintf(msg.proc_name, sizeof(msg.proc_name), "%s", argv[2]);
        send_sentinel_msg(mqid, &msg);
        printf(GREEN "✔ Command sent: Start monitoring '%s' (PID: %d)\n" RESET, argv[2], msg.pid);

    } else if (strcmp(argv[1], "--remove") == 0 && argc == 3) {
        msg.pid = parse_pid(argv[2]);
        if (msg.pid == -1) return 1;
        msg.cmd = CMD_REMOVE;
        send_sentinel_msg(mqid, &msg);
        printf(YELLOW "➜ Command sent: Stop monitoring PID %d\n" RESET, msg.pid);

    } else if (strcmp(argv[1], "--shutdown") == 0) {
        msg.cmd = CMD_SHUTDOWN;
        msg.pid = 0; // Not needed
        send_sentinel_msg(mqid, &msg);
        printf(RED "✖ Command sent: Shut down Sentinel Daemon\n" RESET);

    } else if (strcmp(argv[1], "--monitor") == 0) {
        show_dashboard();

    } else {
        print_usage();
    }

    return 0;
}