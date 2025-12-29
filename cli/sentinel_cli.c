#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "shared_memory.h"
#include "mq.h"

#define SOCKET_PATH "/tmp/process_sentinel.sock"

// ANSI Color Codes
#define RESET  "\x1b[0m"
#define BOLD   "\x1b[1m"
#define RED    "\x1b[31m"
#define GREEN  "\x1b[32m"
#define YELLOW "\x1b[33m"
#define CYAN   "\x1b[36m"
#define CLEAR  "\e[1;1H\e[2J"

// Chat Functionality
void get_system_snapshot(char *buffer, int max_len) {
    int shmid = get_shm_id();
    SentinelBoard* board = attach_shm(shmid);
    if (board == (void*)-1) {
        snprintf(buffer, max_len, "Error: Could not access shared memory.");
        return;
    }

    strcpy(buffer, "Active Processes:\\n"); // Escaped newline for JSON
    for (int i = 0; i < MAX_PROC; i++) {
        if (board->procs[i].active) {
            char line[256];
            char history_str[64] = "[";
            
            // Summarize last 5 history points
            for(int h=0; h<5; h++) {
                char val[8];
                snprintf(val, 8, "%.0f", board->procs[i].cpu_history[h]);
                strcat(history_str, val);
                if(h < 4) strcat(history_str, ",");
            }
            strcat(history_str, "]");

            // We use \\n to denote a newline in the JSON string value
            snprintf(line, sizeof(line), "- %s (PID: %d) | CPU: %.1f%% | Trend: %s | Status: %s\\n",
                     board->procs[i].name, board->procs[i].pid, 
                     board->procs[i].cpu, history_str, board->procs[i].status);
            strncat(buffer, line, max_len - strlen(buffer) - 1);
        }
    }
    detach_shm(board);
}

void start_chat_session() {
    struct sockaddr_un addr;
    char query[512];
    char status[2048];
    char buffer[4096]; // Buffer for JSON and Response
    
    printf(CLEAR);
    printf(BOLD CYAN "====================================================================\n");
    printf("                  SENTINEL AI CHAT ASSISTANT                 \n");
    printf("====================================================================\n" RESET);
    printf("Type 'exit' to quit.\n\n");

    while (1) {
        printf(BOLD "You: " RESET);
        if (!fgets(query, sizeof(query), stdin)) break;
        query[strcspn(query, "\n")] = 0; // Remove newline

        if (strcmp(query, "exit") == 0) break;

        // 1. Get Status
        get_system_snapshot(status, sizeof(status));

        // 2. Build JSON
        // Using manual JSON construction for simplicity in C (escaping is minimal here)
        // Warning: In production, use a JSON library to handle escaping quotes in 'query'
        snprintf(buffer, sizeof(buffer), 
                 "{\"mode\": \"chat\", \"query\": \"%s\", \"status\": \"%s\"}", 
                 query, status);

        // 3. Send to AI
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, SOCKET_PATH);

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            printf(RED "Error: Could not connect to AI Agent. Is it running?\n" RESET);
            close(sock);
            continue;
        }

        send(sock, buffer, strlen(buffer), 0);
        
        // 4. Receive Response
        int n = read(sock, buffer, sizeof(buffer)-1);
        if (n > 0) {
            buffer[n] = '\0';
            printf(GREEN "AI: " RESET "%s\n\n", buffer);
        } else {
            printf(RED "Error: No response from AI.\n" RESET);
        }
        close(sock);
    }
}

// Helper: Safely parse a string to a PID
int parse_pid(const char* str) {
    char* end;
    long pid = strtol(str, &end, 10);
    if (end == str || *end != '\0' || pid <= 0) {
        fprintf(stderr, RED "Error: Invalid PID '%s'. PID must be a positive number.\n" RESET, str);
        return -1;
    }
    return (int)pid;
}

// Helper: Use 'ps' command to find the top CPU consumer
int get_top_cpu_pid(char *name_out) {
    FILE *fp = popen("ps -Ao pid,comm,pcpu --sort=-pcpu --no-headers | head -n 1", "r");
    if (!fp) return -1;

    int pid = -1;
    char comm[256];
    float cpu;

    // Output format:  PID COMMAND %CPU
    if (fscanf(fp, "%d %255s %f", &pid, comm, &cpu) == 3) {
        strcpy(name_out, comm);
    }
    pclose(fp);
    return pid;
}

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
    printf("  ./sentinel_cli --scan\n");
    printf("      " "Auto-detect and monitor the highest CPU process.\n\n");
    printf("  ./sentinel_cli --history " BOLD "<pid>" RESET "\n");
    printf("      " "Show detailed history statistics for a process.\n\n");
    printf("  ./sentinel_cli --rule " BOLD "\"<plain english rule>\"" RESET "\n");
    printf("      " "Add a natural language rule for the AI (e.g., \"Kill chrome if CPU > 80%%\").\n\n");
    printf("  ./sentinel_cli --set-limit " BOLD "<name> <percent>" RESET "\n");
    printf("      " "Add a hard CPU limit for a process (e.g., \"firefox 50\").\n\n");
    printf("  ./sentinel_cli --chat\n");
    printf("      " "Start an interactive chat session with the AI Assistant.\n\n");
    printf("  ./sentinel_cli --clean\n");
    printf("      " "Remove all 'TERMINATED' processes from the dashboard.\n\n");
    printf("  ./sentinel_cli --shutdown\n");
    printf("      " "Signal the Sentinel daemon to shut down gracefully.\n");
}

// Helper: Create a visual bar for percentages
void draw_bar(float percent, char *buffer, int max_len) {
    int bars = (int)(percent / 5.0); // 1 char per 5%
    if (bars > max_len) bars = max_len;
    
    strcpy(buffer, "[");
    for (int i = 0; i < max_len; i++) {
        if (i < bars) strcat(buffer, "|");
        else strcat(buffer, " ");
    }
    strcat(buffer, "]");
}

void show_history(int pid) {
    int shmid = get_shm_id();
    SentinelBoard* board = attach_shm(shmid);
    if (board == (void*)-1) {
        fprintf(stderr, RED "Error: Could not attach to shared memory.\n" RESET);
        return;
    }

    int found = 0;
    for (int i = 0; i < MAX_PROC; i++) {
        if (board->procs[i].active && board->procs[i].pid == pid) {
            found = 1;
            ProcessInfo *p = &board->procs[i];
            
            printf(CLEAR);
            printf(BOLD CYAN "====================================================================\n");
            printf("                  PROCESS HISTORY: %s (PID %d)             \n", p->name, p->pid);
            printf("====================================================================\n" RESET);
            printf("Current Status: %s%s" RESET "\n", 
                   strcmp(p->status, "ANOMALY") == 0 ? RED : GREEN, p->status);
            printf("Latest Advice:  %s\n\n", p->ai_advice);
            
            printf(BOLD "%-10s %-20s %-15s\n" RESET, "TIME AGOT", "CPU USAGE", "RAM USAGE");
            printf("--------------------------------------------------------------------\n");
            
            // History is stored 0=newest, 9=oldest. We print oldest to newest? 
            // Or newest at top? Let's do newest at top (User preference usually).
            for (int j = 0; j < HISTORY_LEN; j++) {
                if (p->ram_history[j] == 0 && p->cpu_history[j] == 0) continue; // Skip empty slots

                char bar[32];
                draw_bar(p->cpu_history[j], bar, 10);
                
                printf("-%-2ds       %-6.2f%% %s %-10ld KB\n", 
                       j * 2, // Assuming ~2s update interval
                       p->cpu_history[j], bar, p->ram_history[j]);
            }
            printf("\n");
            break;
        }
    }

    if (!found) {
        printf(RED "Process PID %d is not currently being monitored.\n" RESET, pid);
    }

    detach_shm(board);
}

void show_dashboard() {
    int shmid = get_shm_id();
    int semid = get_sem_id();
    SentinelBoard* board = attach_shm(shmid);

    if (board == (void*)-1) {
        fprintf(stderr, RED "Error: Could not attach to shared memory. Is the daemon running?\n" RESET);
        return;
    }

    while (1) {
        printf(CLEAR);
        printf(BOLD CYAN "====================================================================\n");
        printf("                PROCESS SENTINEL - AI-POWERED MONITOR               \n");
        printf("====================================================================\n" RESET);
        printf(BOLD "%-8s %-16s %-16s %-10s %-10s\n" RESET, "PID", "NAME", "CPU USAGE", "RAM(KB)", "STATUS");
        printf("--------------------------------------------------------------------\n");

        sem_lock(semid);
        int active_count = 0;
        for (int i = 0; i < MAX_PROC; i++) {
            if (board->procs[i].active) {
                active_count++;
                char *color = GREEN;
                if (strcmp(board->procs[i].status, "ANOMALY") == 0) color = RED;
                else if (strcmp(board->procs[i].status, "PAUSED") == 0) color = YELLOW;
                else if (strcmp(board->procs[i].status, "TERMINATED") == 0) color = "\x1b[90m"; // Gray

                char cpu_bar[32];
                draw_bar(board->procs[i].cpu, cpu_bar, 8);

                // Line 1: Stats
                printf("% -8d %-16s %s%-6.2f%% %s %-10ld %-10s" RESET "\n", 
                       board->procs[i].pid, board->procs[i].name, color,
                       board->procs[i].cpu, cpu_bar, board->procs[i].ram, board->procs[i].status);
                
                // Line 2: AI Advice (Full width)
                if (strlen(board->procs[i].ai_advice) > 0 && strcmp(board->procs[i].ai_advice, "N/A") != 0) {
                    printf("   %s└─ AI: %s%s\n", BOLD, board->procs[i].ai_advice, RESET);
                }
                printf("\n"); // Spacer
            }
        }
        sem_unlock(semid);

        if (active_count == 0) {
            printf("\n  (No processes being monitored. Use --add or --scan)\n\n");
        }

        printf("====================================================================\n");
        printf("Refresh: 1s | Ctrl+C to Exit\n");
        sleep(1);
    }
    detach_shm(board);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    int mqid = init_mq();
    SentinelMsg msg;

    if (strcmp(argv[1], "--history") == 0 && argc == 3) {
        int pid = parse_pid(argv[2]);
        if (pid != -1) show_history(pid);
        return 0; // Don't send MQ message, just read SHM
    }

    if (strcmp(argv[1], "--chat") == 0) {
        start_chat_session();
        return 0;
    }

    if (strcmp(argv[1], "--add") == 0 && argc == 3) {
        msg.pid = parse_pid(argv[2]);
        if (msg.pid == -1) return 1;
        msg.cmd = CMD_ADD;
        snprintf(msg.proc_name, sizeof(msg.proc_name), "pid_%d", msg.pid); // Generic name
        if (send_sentinel_msg(mqid, &msg) == 0) {
            printf(GREEN "✔ Command sent: Start monitoring PID %d\n" RESET, msg.pid);
        } else {
             fprintf(stderr, RED "Error: Failed to send command.\n" RESET);
        }

    } else if (strcmp(argv[1], "--add-by-name") == 0 && argc == 3) {
        msg.pid = find_pid_by_name(argv[2]);
        if (msg.pid == -1) {
            fprintf(stderr, RED "Error: Process '%s' not found.\n" RESET, argv[2]);
            return 1;
        }
        msg.cmd = CMD_ADD;
        snprintf(msg.proc_name, sizeof(msg.proc_name), "%s", argv[2]);
        if (send_sentinel_msg(mqid, &msg) == 0) {
            printf(GREEN "✔ Command sent: Start monitoring '%s' (PID: %d)\n" RESET, argv[2], msg.pid);
        } else {
             fprintf(stderr, RED "Error: Failed to send command.\n" RESET);
        }

    } else if (strcmp(argv[1], "--scan") == 0) {
        char name[256] = "unknown";
        msg.pid = get_top_cpu_pid(name);
        if (msg.pid == -1) {
            fprintf(stderr, RED "Error: Could not scan processes (is 'ps' installed?).\n" RESET);
            return 1;
        }
        msg.cmd = CMD_ADD;
        // Fix: Use %.63s to prevent buffer overflow/truncation warning
        snprintf(msg.proc_name, sizeof(msg.proc_name), "%.63s", name);
        if (send_sentinel_msg(mqid, &msg) == 0) {
            printf(GREEN "✔ Auto-Scan: Found top process '%s' (PID: %d). Monitoring started.\n" RESET, name, msg.pid);
        } else {
             fprintf(stderr, RED "Error: Failed to send command.\n" RESET);
        }

    } else if (strcmp(argv[1], "--clean") == 0) {
        msg.cmd = CMD_CLEAN;
        msg.pid = 0; 
        send_sentinel_msg(mqid, &msg);
        printf(GREEN "✔ Command sent: Clean up terminated processes.\n" RESET);

    } else if (strcmp(argv[1], "--remove") == 0 && argc == 3) {
        msg.pid = parse_pid(argv[2]);
        if (msg.pid == -1) return 1;
        msg.cmd = CMD_REMOVE;
        send_sentinel_msg(mqid, &msg);
        printf(YELLOW "➜ Command sent: Stop monitoring PID %d\n" RESET, msg.pid);
    
    } else if (strcmp(argv[1], "--shutdown") == 0) {
        msg.cmd = CMD_SHUTDOWN;
        msg.pid = 0;
        send_sentinel_msg(mqid, &msg);
        printf(RED "✖ Command sent: Shut down Sentinel Daemon\n" RESET);

    } else if (strcmp(argv[1], "--rule") == 0 && argc == 3) {
        FILE *f = fopen("ai/user_rules.txt", "a");
        if (!f) {
            fprintf(stderr, RED "Error: Could not open rules file (ai/user_rules.txt).\n" RESET);
            return 1;
        }
        fprintf(f, "- %s\n", argv[2]);
        fclose(f);
        printf(GREEN "✔ Rule added: \"%s\"\n" RESET, argv[2]);
        printf(YELLOW "  (The AI will consider this rule in future decisions)\n" RESET);

    } else if (strcmp(argv[1], "--set-limit") == 0 && argc == 4) {
        // ./sentinel_cli --set-limit chrome 50
        FILE *f = fopen("rules.conf", "a");
        if (!f) {
            fprintf(stderr, RED "Error: Could not open rules.conf.\n" RESET);
            return 1;
        }
        int limit = atoi(argv[3]);
        if (limit <= 0 || limit > 100) {
             fprintf(stderr, RED "Error: Limit must be between 1 and 100.\n" RESET);
             fclose(f);
             return 1;
        }
        fprintf(f, "%s %d\n", argv[2], limit);
        fclose(f);
        printf(GREEN "✔ Hard Limit added: %s > %d%%\n" RESET, argv[2], limit);
        printf(YELLOW "  (Restart the daemon to apply this rule)\n" RESET);

    } else if (strcmp(argv[1], "--monitor") == 0) {
        show_dashboard();

    } else {
        print_usage();
    }

    return 0;
}
