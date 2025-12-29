#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

// Helper to restart a process by reading its original cmdline
int attempt_auto_restart(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(" -> Failed to read process cmdline for restart");
        return -1;
    }

    // Read full cmdline (args are separated by \0)
    char buffer[2048];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (len == 0) return -1;
    
    // Parse cmdline into argv array
    char *argv[64];
    int argc = 0;
    char *ptr = buffer;
    char *end = buffer + len;
    
    while (ptr < end && argc < 63) {
        argv[argc++] = ptr;
        ptr += strlen(ptr) + 1;
    }
    argv[argc] = NULL; // Null terminate list

    printf(" -> Auto-Restarting: %s\n", argv[0]);

    // 1. Kill the old process
    kill(pid, SIGKILL);
    usleep(100000); // Wait 100ms for cleanup

    // 2. Fork and Exec new process
    pid_t new_pid = fork();
    if (new_pid == 0) {
        // Child Process
        setsid(); // Detach from daemon's session
        // Redirect stdout/stderr to /dev/null so it doesn't clutter daemon logs
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        
        execvp(argv[0], argv);
        exit(1); // Exec failed
    } else if (new_pid > 0) {
        printf(" -> Successfully launched new instance (PID: %d)\n", new_pid);
        return new_pid;
    } else {
        perror(" -> Fork failed");
        return -1;
    }
}

int execute_ai_action(int pid, const char *ai_response) {
    char action[32] = {0};

    // Improved Parser: Look for "ACTION:" and extract the word following it
    const char *ptr = strstr(ai_response, "ACTION:");
    if (ptr) {
        ptr += 7; // Move past "ACTION:"
        // Skip any spaces or brackets
        while (*ptr && (isspace(*ptr) || *ptr == '[')) ptr++;
        
        int i = 0;
        while (i < 31 && isalpha(*ptr)) {
            action[i++] = toupper(*ptr++);
        }
        action[i] = '\0';
    }

    // Default to IGNORE if nothing found
    if (action[0] == '\0') strcpy(action, "IGNORE");

    printf("[RECOVERY] AI Decision: %s (PID %d)\n", action, pid);

    if (strcmp(action, "KILL") == 0) {
        if (kill(pid, SIGKILL) == 0) {
            printf(" -> Forcefully terminated PID %d.\n", pid);
        } else {
            perror(" -> Failed to kill process");
        }
    }
    else if (strcmp(action, "PAUSE") == 0) {
        if (kill(pid, SIGSTOP) == 0) {
            printf(" -> Process %d paused (SIGSTOP).\n", pid);
        } else {
            perror(" -> Failed to pause");
        }
    }
    else if (strcmp(action, "RENICE") == 0) {
        if (setpriority(PRIO_PROCESS, pid, 19) == 0) {
            printf(" -> Lowered priority of PID %d to 19 (lowest).\n", pid);
        } else {
            perror(" -> Failed to renice (permission?)");
        }
    }
    else if (strcmp(action, "RESTART") == 0) {
        return attempt_auto_restart(pid);
    }
    else if (strcmp(action, "IGNORE") == 0 || strcmp(action, "MONITOR") == 0) {
        printf(" -> No action taken — monitoring continues.\n");
    }
    else {
        printf(" -> Unknown AI command '%s' — ignoring.\n", action);
    }
    return 0; // No PID change
}