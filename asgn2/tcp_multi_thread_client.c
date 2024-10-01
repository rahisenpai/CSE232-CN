#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8005

void* client_task(void* arg) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    const char* request = "Request: Top CPU Processes";

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return NULL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return NULL;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return NULL;
    }

    send(sock, request, strlen(request), 0);
    printf("Client: Request sent to server\n");

    // Read the server's response (information about top CPU-consuming processes)
    read(sock, buffer, 1024);
    printf("Client: Server response:\n%s\n", buffer);

    close(sock);
    return NULL;
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_clients>\n", argv[0]);
        return 1;
    }

    int num_clients = atoi(argv[1]);
    pthread_t threads[num_clients];

    // Create multiple threads for concurrent client connections
    for (int i = 0; i < num_clients; i++) {
        pthread_create(&threads[i], NULL, client_task, NULL);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}