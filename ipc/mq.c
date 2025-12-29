#include "mq.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// Create or connect to the message queue
int init_mq() {
    // IPC_CREAT: Create if doesn't exist
    // 0666: Permissions (Read/Write for everyone)
    int mqid = msgget(SENTINEL_MQ_KEY, IPC_CREAT | 0666);
    if (mqid == -1) {
        perror("msgget failed");
        return -1;
    }
    return mqid;
}

// CLI uses this to send a command
int send_sentinel_msg(int mqid, SentinelMsg *msg) {
    // mtype must be > 0
    msg->mtype = 1; 

    // msgsnd parameters: (queue id, message pointer, size of data, flags)
    // Note: size is the struct MINUS the 'long mtype'
    if (msgsnd(mqid, msg, sizeof(SentinelMsg) - sizeof(long), 0) == -1) {
        perror("msgsnd failed");
        return -1;
    }
    return 0;
}

// Daemon uses this to wait for commands
int receive_sentinel_msg(int mqid, SentinelMsg *msg) {
    // msgrcv blocks until a message of type 1 is available
    if (msgrcv(mqid, msg, sizeof(SentinelMsg) - sizeof(long), 1, 0) == -1) {
        if (errno == EINTR) return -1; // Interrupted by signal
        perror("msgrcv failed");
        return -1;
    }
    return 0;
}

// Clean up the queue (usually called by Daemon on exit)
void cleanup_mq(int mqid) {
    if (msgctl(mqid, IPC_RMID, NULL) == -1) {
        perror("msgctl RMID failed");
    } else {
        printf("Message Queue cleaned up successfully.\n");
    }
}

void force_cleanup_mq() {
    int mqid = msgget(SENTINEL_MQ_KEY, 0666);
    if (mqid != -1) {
        if (msgctl(mqid, IPC_RMID, NULL) == -1) {
            perror("[IPC] Warning: Failed to remove stale MQ");
        } else {
            printf("[IPC] Stale MQ removed (ID: %d).\n", mqid);
        }
    }
}