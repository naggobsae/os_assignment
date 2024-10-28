#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define FIFO_NAME "/tmp/server_fifo"
#define MAXLINE 4096

void handle_client_request(char *request);

int main() {
    char buff[MAXLINE];
    int server_fd;

    // Create FIFO for server
    if (mkfifo(FIFO_NAME, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo error");
        exit(1);
    }

    printf("Server is running and waiting for clients...\n");

    while (1) {
        // Open the server FIFO for reading
        if ((server_fd = open(FIFO_NAME, O_RDONLY)) < 0) {
            perror("open error");
            exit(1);
        }
        printf("Server FIFO opened for reading.\n");

        // Read client requests
        while (read(server_fd, buff, MAXLINE) > 0) {
            printf("Received client request: %s\n", buff);

            pid_t child_pid = fork();  // Fork a child process

            if (child_pid == 0) {  // Child process handles the client request
                handle_client_request(buff);
                exit(0);  // Exit child process
            } else if (child_pid < 0) {
                perror("Fork error");
            } else {
                printf("Forked child process with PID: %d\n", child_pid);
            }
        }

        close(server_fd);  // Close the server FIFO after reading
        printf("Server FIFO closed, reopening for next request.\n");
    }

    unlink(FIFO_NAME);  // Remove FIFO if server ends
    printf("Server FIFO unlinked and server exiting.\n");
    return 0;
}

void handle_client_request(char *request) {
    char filename[256], access_type, data[MAXLINE];
    int bytes_to_read, file_fd, n;
    char response_fifo[256];

    // Parse client request
    sscanf(request, "%s %c %d %[^\n]", filename, &access_type, &bytes_to_read, data);
    printf("Parsed request - Filename: %s, Access Type: %c, Bytes to read/write: %d, Data: %s\n", filename, access_type, bytes_to_read, data);

    // Create response FIFO based on the client PID
    snprintf(response_fifo, sizeof(response_fifo), "/tmp/fifo_%d", getpid());
    if (mkfifo(response_fifo, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo error for response FIFO");
        exit(1);
    }

    printf("Response FIFO created: %s\n", response_fifo);

    // 지연 추가: 클라이언트가 응답 FIFO를 열 수 있는 시간 확보
    usleep(500000);  // 0.5초 지연

    int response_fd = open(response_fifo, O_WRONLY);
    if (response_fd < 0) {
        perror("open error for response FIFO");
        unlink(response_fifo);
        return;
    }
    printf("Response FIFO opened for writing.\n");

    // Handle read mode
    if (access_type == 'r') {
        if ((file_fd = open(filename, O_RDONLY)) < 0) {
            snprintf(data, sizeof(data), "Error opening file: %s\n", strerror(errno));
            write(response_fd, data, strlen(data));
            printf("Error response sent to client: %s\n", data);
        } else {
            n = read(file_fd, data, bytes_to_read);
            if (n >= 0) {
                write(response_fd, data, n);  // Send read data to client
                printf("Data read from file and sent to client: %s\n", data);
            } else {
                snprintf(data, sizeof(data), "Error reading file: %s\n", strerror(errno));
                write(response_fd, data, strlen(data));
                printf("Error reading file, error response sent to client: %s\n", data);
            }
            close(file_fd);
            printf("File closed after read.\n");
        }
    }
    // Handle write mode
    else if (access_type == 'w') {
        if ((file_fd = open(filename, O_WRONLY | O_CREAT, 0666)) < 0) {
            snprintf(data, sizeof(data), "Error opening file: %s\n", strerror(errno));
            write(response_fd, data, strlen(data));
            printf("Error response sent to client: %s\n", data);
        } else {
            n = write(file_fd, data, strlen(data));
            if (n >= 0) {
                snprintf(data, sizeof(data), "Written %d bytes to %s\n", n, filename);
                write(response_fd, data, strlen(data));  // Send write confirmation to client
                printf("Write confirmation sent to client: %s\n", data);
            } else {
                snprintf(data, sizeof(data), "Error writing to file: %s\n", strerror(errno));
                write(response_fd, data, strlen(data));
                printf("Error writing to file, error response sent to client: %s\n", data);
            }
            close(file_fd);
            printf("File closed after write.\n");
        }
    } else {
        snprintf(data, sizeof(data), "Invalid access type: %c", access_type);
        write(response_fd, data, strlen(data));
        printf("Invalid access type error response sent to client.\n");
    }

    close(response_fd);  // Close the response FIFO
    unlink(response_fifo);  // Remove response FIFO
    printf("Response FIFO %s closed and unlinked.\n", response_fifo);
}
