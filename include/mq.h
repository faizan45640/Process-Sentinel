#ifndef MQ_H
#define MQ_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

// Unique key for the message queue (can be any random integer)
#define SENTINEL_MQ_KEY 0x12345 

// Command types
typedef enum {
    CMD_ADD = 1,    // Add a process to monitoring
    CMD_REMOVE,     // Remove a process
    CMD_SUSPEND,    // Pause monitoring for a PID
    CMD_RESUME,     // Resume monitoring for a PID
    CMD_SHUTDOWN    // Tell the daemon to exit
} CommandType;

// The Message Structure
// IMPORTANT: The first member MUST be 'long mtype'
typedef struct {
    long mtype;             // Target type (1 for general commands)
    CommandType cmd;        // The actual action
    int pid;                // Target Process ID
    char proc_name[64];     // Name for logging/AI context
} SentinelMsg;

// Function prototypes
int init_mq();              // Create or get the queue
int send_sentinel_msg(int mqid, SentinelMsg *msg);
int receive_sentinel_msg(int mqid, SentinelMsg *msg);
void cleanup_mq(int mqid);  // Delete the queue from the system

#endif