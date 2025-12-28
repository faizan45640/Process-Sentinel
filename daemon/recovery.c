#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <errno.h>

void execute_ai_action(int pid, const char *ai_response) {
    char action[32] = {0};  // Bigger buffer just in case

    // Extract action from "ACTION:[XXXXX]"
    const char *start = strstr(ai_response, "ACTION:[");
    if (start && strlen(start) > 8) {
        start += 8;  // Skip "ACTION:["
        const char *end = strchr(start, ']');
        if (end && end > start) {
            size_t len = end - start;
            if (len >= sizeof(action)) len = sizeof(action) - 1;
            strncpy(action, start, len);
            action[len] = '\0';  // Critical: null terminate!
        }
    }

    // If no valid action found, default to ignore
    if (action[0] == '\0') {
        strcpy(action, "IGNORE");
    }

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
        if (kill(pid, SIGKILL) == 0) {
            printf(" -> Killed PID %d — ready for manual or scripted restart.\n", pid);
            // Future: system("google-chrome &"); or execve(original_cmd);
        } else {
            perror(" -> Failed to kill for restart");
        }
    }
    else if (strcmp(action, "IGNORE") == 0) {
        printf(" -> No action taken — monitoring continues.\n");
    }
    else {
        printf(" -> Unknown AI command '%s' — ignoring.\n", action);
    }
}