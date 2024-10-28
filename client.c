#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define FIFO_NAME "/tmp/server_fifo"
#define MAXLINE 4096

int main() {
    char filename[256], access_type, buff[MAXLINE];
    int bytes_to_read;
    char data[MAXLINE];
    char line[MAXLINE];

    // 서버와 통신할 FIFO 열기
    //printf("[DEBUG] Opening FIFO: %s\n", FIFO_NAME);
    int server_fd = open(FIFO_NAME, O_WRONLY);
    if (server_fd < 0) {
        perror("open error");
        exit(1);
    }
    printf("Successfully opened server FIFO.\n");

    while (1) {
        // 파일 이름 입력
        printf("Enter file name (or 'exit' to quit): ");
        fgets(filename, sizeof(filename), stdin);
        filename[strcspn(filename, "\n")] = '\0';

        printf("Filename entered: %s\n", filename);

        if (strcmp(filename, "exit") == 0) {
            printf("Client is exiting...\n");
            break;
        }

        printf("Enter access type (r for read, w for write): ");
        fgets(line, sizeof(line), stdin);
        sscanf(line, " %c", &access_type);

        printf("Access type entered: %c\n", access_type);

        if (access_type == 'r') {
            printf("Enter number of bytes to read: ");
            fgets(line, sizeof(line), stdin);
            sscanf(line, "%d", &bytes_to_read);

            if (snprintf(buff, sizeof(buff), "%s %c %d", filename, access_type, bytes_to_read) >= sizeof(buff)) {
                fprintf(stderr, "Error: buffer overflow\n");
                continue;
            }
        } else if (access_type == 'w') {
            printf("Enter data to write (Press Enter on empty line to finish):\n");
            data[0] = '\0';

            while (1) {
                printf(">> ");
                fgets(line, MAXLINE, stdin);

                printf("Line entered: %s\n", line);

                if (strcmp(line, "\n") == 0) {
                    printf("End of data input.\n");
                    break;
                }

                if (strlen(data) + strlen(line) >= MAXLINE) {
                    fprintf(stderr, "Error: input too long\n");
                    break;
                }

                strncat(data, line, sizeof(data) - strlen(data) - 1);
            }

            printf("Final data to write: %s\n", data);

            if (snprintf(buff, sizeof(buff), "%s %c 0 %s", filename, access_type, data) >= sizeof(buff)) {
                fprintf(stderr, "Error: buffer overflow\n");
                continue;
            }
        } else {
            printf("Invalid access type!\n");
            continue;
        }

        // 응답 받을 FIFO 생성 및 먼저 열기
        char response_fifo[256];
        snprintf(response_fifo, sizeof(response_fifo), "/tmp/fifo_%d", getpid());
        printf("Creating response FIFO: %s\n", response_fifo);

        if (mkfifo(response_fifo, 0666) < 0 && errno != EEXIST) {
            perror("mkfifo error");
            exit(1);
        }

        int response_fd = open(response_fifo, O_RDONLY | O_NONBLOCK);
        if (response_fd < 0) {
            perror("open error");
            unlink(response_fifo);
            continue;
        }
        printf("Response FIFO opened for reading: %s\n", response_fifo);

        // 서버로 요청 전송
        //printf("[DEBUG] Buffer to send to server: %s\n", buff);
        if (write(server_fd, buff, strlen(buff)) == -1) {
            perror("write error");
            close(response_fd);
            unlink(response_fifo);
            continue;
        }
        printf("Data successfully written to server.\n");

        // 서버 응답 읽기
        int n;
        while ((n = read(response_fd, buff, MAXLINE)) <= 0) {
            usleep(100000);  // 0.1초 대기하며 다시 읽기 시도
        }

        if (n > 0) {
            buff[n] = '\0';  // Null-terminate the response
            printf("Server response: %s\n", buff);
        } else {
            printf("Failed to read response from server.\n");
        }

        // 응답 FIFO 삭제
        close(response_fd);
        unlink(response_fifo);
        printf("Response FIFO %s deleted.\n", response_fifo);
    }

    close(server_fd);
    printf("Client closed server connection.\n");
    return 0;
}
