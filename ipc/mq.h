// mq.h
#define MSG_QUEUE_KEY 1234
#define MAX_TEXT 512

typedef enum {
    CMD_ADD_PROCESS,    // Start monitoring a PID
    CMD_REMOVE_PROCESS, // Stop monitoring a PID
    CMD_STATUS          // Get list of monitored processes
} CommandType;

typedef struct {
    long msg_type;      // Must be > 0 for POSIX MQ
    CommandType command;
    int pid;
    char name[64];
} SentinelMessage;