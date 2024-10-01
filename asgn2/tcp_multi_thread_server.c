//header files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>

#define PORT 8005

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


// Function to handle the client request
void* handle_client(void* socket_desc) {
    int new_socket = *(int*)socket_desc;
    char buffer[1024] = {0};
    char response[1024] = {0};

    read(new_socket, buffer, 1024);
    printf("Server: Message received: %s\n", buffer);

    // Get the top CPU-consuming processes
    get_top_cpu_processes(response);

    // Send the information to the client
    send(new_socket, response, strlen(response), 0);
    printf("Server: Response sent\n");

    close(new_socket);
    free(socket_desc);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

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

    // Continuously accept incoming connections and handle them in separate threads
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // Allocate memory for socket descriptor and pass it to the thread
        int* socket_desc = malloc(sizeof(int));
        *socket_desc = new_socket;

        // Create a new thread to handle the client connection
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)socket_desc) != 0) {
            perror("Thread creation failed");
            free(socket_desc);
            exit(EXIT_FAILURE);
        }

        // Detach the thread so it can clean up after itself when done
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}