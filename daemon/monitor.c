#include<string.h>
//#include<unistd.h>

unsigned long get_Total_ticks(int pid){
    char path[40];
    unsigned long utime, stime;
    long rss;
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    char comm[256] ,state;
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }
    fscanf(fp,
    "%d %s %c "
    "%*d %*d %*d %*d %*d "
    "%*u %*u %*u %*u %*u "
    "%lu %lu "
    "%*d %*d %*d %*d %*d %*d "
    "%*u "
    "%*u "
    "%*d "
    "%ld",
    &pid, comm, &state,
    &utime, &stime,
    &rss
);
    fclose(file);
    return utime + stime;
}

long get_ram_usage(int pid){
    char path[40] , line[128];
    long rss = 0;

    sprintf(path, "/proc/%d/statm", pid);
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }
     while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %ld", &rss); // Read the number after VmRSS:
            break;
        }
    }
    fclose(file);
    return rss; // in KB
}