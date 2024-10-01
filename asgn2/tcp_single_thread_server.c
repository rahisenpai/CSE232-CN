#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include  <errno.h>
#include <dirent.h>

#define PORT 8005
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 50

void get_top_cpu_processes(char* response) {
    DIR* dir;
    struct dirent* entry;
    char path[512], process_name[256];
    int pid, user_time, kernel_time, total_time;
    FILE* stat_file;

    // Struct to store process information
    struct {
        int pid;
        char name[256];
        int cpu_time;
    } top1 = {0, "", 0}, top2 = {0, "", 0};

    // Open the /proc directory
    if ((dir = opendir("/proc")) == NULL) {
        perror("Failed to open /proc");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {
            // Build path to /proc/[pid]/stat
            snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);

            // Open the /proc/[pid]/stat file
            stat_file = fopen(path, "r");
            if (stat_file) {
                fscanf(stat_file, "%d %s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %d %d",
                       &pid, process_name, &user_time, &kernel_time);
                total_time = user_time + kernel_time;
                fclose(stat_file);

                // Check if this process is one of the top two CPU consumers
                if (total_time > top1.cpu_time) {
                    top2 = top1;
                    top1.pid = pid;
                    strcpy(top1.name, process_name);
                    top1.cpu_time = total_time;
                } else if (total_time > top2.cpu_time) {
                    top2.pid = pid;
                    strcpy(top2.name, process_name);
                    top2.cpu_time = total_time;
                }
            }
        }
    }
    closedir(dir);

    // Return the top 2 processes
    snprintf(response, 1024,
             "Top 2 CPU-consuming processes:\n"
             "1. Process: %s, PID: %d, CPU time: %d ticks\n"
             "2. Process: %s, PID: %d, CPU time: %d ticks\n",
             top1.name, top1.pid, top1.cpu_time,
             top2.name, top2.pid, top2.cpu_time);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    char response[BUFFER_SIZE] = {0};
    fd_set readfds;
    int max_sd, activity;
    int client_sockets[MAX_CLIENTS] = {0}; // Array to hold client sockets

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) {
        // Clear the socket set
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            // Socket descriptor
            int sd = client_sockets[i];

            // If valid socket descriptor, then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            // Keep track of the maximum socket descriptor
            if (sd > max_sd)
                max_sd = sd;
        }

        // Wait for an activity on one of the sockets
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("Select error");
        }

        // If something happened on the master socket, then it's an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            printf("New connection: socket fd is %d, ip is : %s, port: %d\n",
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Add new socket to array of sockets
            for (int i = 0; i < MAX_CLIENTS; i++) {
                // If position is empty
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // Check all clients for incoming data
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                // Check if it was for closing, and also read the incoming message
                int valread = read(sd, buffer, BUFFER_SIZE);
                if (valread == 0) {
                    // Somebody disconnected, get their details and print
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Host disconnected, ip %s, port %d\n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    printf("Message received from client: %s\n", buffer);

                    // Get the top CPU-consuming processes
                    get_top_cpu_processes(response);

                    // Send the response back to the client
                    send(sd, response, strlen(response), 0);
                    printf("Response sent to client\n");
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
