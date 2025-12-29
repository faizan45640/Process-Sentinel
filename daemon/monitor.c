#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>     // for kill()
#include "monitor.h"

// Fixed: Name matches what we call later
unsigned long get_process_ticks(int pid) {
    char path[40];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    char buffer[1024];
    if (!fgets(buffer, sizeof(buffer), file)) {
        fclose(file);
        return 0;
    }
    fclose(file);

    // Find the last ')' to correctly handle process names with spaces/parens
    char *right_paren = strrchr(buffer, ')');
    if (!right_paren) return 0;

    // The string after ')' contains the stats we need
    // Format: state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime
    // Indices relative to 'right_paren + 2' (skip ") "):
    // 0: state (%c)
    // 1: ppid (%d)
    // ...
    // 11: utime (%lu)
    // 12: stime (%lu)

    char state;
    unsigned long utime = 0, stime = 0;
    
    // We use sscanf to skip fields. 
    // The format string starts matching from the character AFTER the space following ')'
    // so we pointer arithmetic: right_paren + 2.
    // Safety check: ensure we don't go out of bounds
    if (strlen(right_paren) < 2) return 0;

    int ret = sscanf(right_paren + 2, 
        "%c "           // state
        "%*d %*d %*d %*d %*d " // ppid, pgrp, session, tty_nr, tpgid
        "%*u %*u %*u %*u %*u " // flags, minflt, cminflt, majflt, cmajflt
        "%lu %lu",             // utime, stime
        &state, &utime, &stime
    );

    if (ret != 3) { // We expect state, utime, stime
        return 0; 
    }

    return utime + stime;
}

// Fixed: Use /proc/<pid>/status for accurate VmRSS in KB (not statm!)
long get_ram_usage(int pid) {
    char path[40];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char line[128];
    long rss = -1;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %ld", &rss);
            break;
        }
    }

    fclose(file);
    return rss;  // Returns resident memory in KB, or -1 on error
}

// Correct and working
unsigned long get_system_total_ticks(void) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0;

    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
    int ret = fscanf(fp, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
                     &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    fclose(fp);

    if (ret < 4) return 0;

    return user + nice + system + idle + iowait + irq + softirq + steal;
}

// Main function — now solid and accurate
int get_process_stats(int pid, ProcessStats *stats) {
    if (pid <= 0 || stats == NULL) return -1;

    // Quick check: does the process even exist?
    if (kill(pid, 0) == -1) {
        return -1;
    }

    // Snapshot 1
    unsigned long proc_ticks1 = get_process_ticks(pid);
    unsigned long sys_ticks1  = get_system_total_ticks();

    if (proc_ticks1 == 0 || sys_ticks1 == 0) {
        return -1;
    }

    // Wait 500ms for meaningful difference
    usleep(500000);

    // Snapshot 2
    unsigned long proc_ticks2 = get_process_ticks(pid);
    unsigned long sys_ticks2  = get_system_total_ticks();

    if (proc_ticks2 == 0 || sys_ticks2 == 0) {
        return -1;
    }

    // Calculate CPU usage over the interval
    unsigned long proc_delta = proc_ticks2 - proc_ticks1;
    unsigned long sys_delta  = sys_ticks2  - sys_ticks1;

    if (sys_delta == 0) {
        stats->cpu_usage = 0.0f;
    } else {
        stats->cpu_usage = 100.0f * (float)proc_delta / (float)sys_delta;
    }

    // Get RAM usage in KB
    stats->rss = get_ram_usage(pid);

    stats->pid = pid;

    return 0;  // Success
}