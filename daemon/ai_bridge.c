#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define SOCKET_PATH "/tmp/process_sentinel.sock"

void ask_ai_for_help(int pid, const char* name, float cpu, float mean, float stddev, char* result_out) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        strcpy(result_out, "ACTION:[IGNORE] REASON:[AI Offline]");
        close(sock);
        return;
    }

    char buffer[1024];
    float z = (stddev > 0) ? (cpu - mean) / stddev : 0;
    snprintf(buffer, 1024, "{\"pid\": %d, \"name\": \"%s\", \"cpu\": %.2f, \"mean\": %.2f, \"stddev\": %.2f, \"z_score\": %.2f}",
             pid, name, cpu, mean, stddev, z);

    send(sock, buffer, strlen(buffer), 0);
    int n = read(sock, buffer, 1024);
    buffer[n] = '\0';
    strcpy(result_out, buffer);
    close(sock);
}