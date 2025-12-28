// monitor.h
#ifndef MONITOR_H
#define MONITOR_H
typedef struct {
    int pid;
    char name[256];
    unsigned long utime;  // User time (CPU)
    unsigned long stime;  // System time (CPU)
    long rss;             // Resident Set Size (RAM in KB)
    double cpu_usage;     // Calculated %
} ProcessStats;


int get_process_stats(int pid, ProcessStats *stats);

#endif // MONITOR_H