#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>

#define SOCKET_PATH "/tmp/process_sentinel.sock"

void ask_ai_for_help(int pid, const char* name, float cpu, float mean, float stddev) {
    int sock = 0;
    struct sockaddr_un serv_addr;
    char buffer[1024];

    // 1. Create Socket
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, SOCKET_PATH);

    // 2. Connect to Python AI Agent
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("AI Agent not running");
        return;
    }

    // 3. Format Anomaly Data as JSON
    // We manually build the JSON string to avoid heavy libraries
    float z_score = (stddev > 0) ? (cpu - mean) / stddev : 0;
    snprintf(buffer, sizeof(buffer), 
        "{\"pid\": %d, \"name\": \"%s\", \"cpu\": %.2f, \"mean\": %.2f, \"stddev\": %.2f, \"z_score\": %.2f}",
        pid, name, cpu, mean, stddev, z_score);

    // 4. Send to Python
    send(sock, buffer, strlen(buffer), 0);

    // 5. Receive AI's Recommendation
    int valread = read(sock, buffer, 1024);
    buffer[valread] = '\0';
    
    printf("\n[DAEMON] Received AI Instruction: %s\n", buffer);

    close(sock);
}