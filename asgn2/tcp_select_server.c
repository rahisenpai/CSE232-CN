#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

#define PORT 8005
#define MAX_CLIENTS 500

void get_top_cpu_processes(char* response) {
    DIR* dir;
    struct dirent* entry;
    char path[512], process_name[256];
    int pid, user_time, kernel_time, total_time;
    FILE* stat_file;

    struct {
        int pid;
        char name[256];
        int cpu_time;
    } top1 = {0, "", 0}, top2 = {0, "", 0};

    if ((dir = opendir("/proc")) == NULL) {
        perror("Failed to open /proc");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {
            snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
            stat_file = fopen(path, "r");
            if (stat_file) {
                fscanf(stat_file, "%d %s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %d %d",
                       &pid, process_name, &user_time, &kernel_time);
                total_time = user_time + kernel_time;
                fclose(stat_file);

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

    snprintf(response, 1024,
             "Top 2 CPU-consuming processes:\n"
             "1. Process: %s, PID: %d, CPU time: %d ticks\n"
             "2. Process: %s, PID: %d, CPU time: %d ticks\n",
             top1.name, top1.pid, top1.cpu_time,
             top2.name, top2.pid, top2.cpu_time);
}

int main() {
    int server_fd, new_socket, max_sd, activity, i;
    struct sockaddr_in address;
    fd_set readfds;
    char buffer[1024] = {0};
    char response[1024] = {0};
    int addrlen = sizeof(address);
    int client_sockets[MAX_CLIENTS] = {0}; // Array to hold client socket descriptors

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Add child sockets to set
        for (i = 0; i < MAX_CLIENTS; i++) {
            // If valid socket descriptor then add to read list
            if (client_sockets[i] > 0)
                FD_SET(client_sockets[i], &readfds);
            if (client_sockets[i] > max_sd)
                max_sd = client_sockets[i];
        }

        // Wait for activity on one of the sockets
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            printf("Select error");
        }

        // If something happened on the master socket, then it's an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&(addrlen))) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }
            // Add new socket to array of sockets
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // Check for I/O operations on other sockets
        for (i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                // Read the incoming message
                int valread = read(sd, buffer, 1024);
                if (valread == 0) {
                    // Client disconnected
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&(addrlen));
                    printf("Host disconnected, ip: %s, port: %d\n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    // Process the request and send a response
                    get_top_cpu_processes(response);
                    send(sd, response, strlen(response), 0);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}