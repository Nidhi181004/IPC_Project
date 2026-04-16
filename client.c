/* client.c
Author      : Nidhi Goswami
Date        : 15-04-2026
Description : Client process for IPC Chat Application.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#define SERVER_FIFO "server_pipe"
#define BUFFER_SIZE 1024

char fifo_name[100];
char username_global[50];
char input_buffer[256] = "";

typedef struct {
    char username[50];
    char message[256];
    char target[50];
    int type;
} Packet;

void* send_thread(void *arg)
{
    Packet pkt;
    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
            continue;

        input_buffer[strcspn(input_buffer, "\n")] = 0;
        if (strlen(input_buffer) == 0)
            continue;

        strcpy(pkt.username, (char*)arg);
        pkt.type = 1;
        strcpy(pkt.target, "ALL");
        strcpy(pkt.message, input_buffer);

        // Handle private message @username
        if (input_buffer[0] == '@') {
            char *space = strchr(input_buffer, ' ');
            if (space != NULL) {
                int len = space - input_buffer - 1;
                if (len > 0 && len < 49) {
                    strncpy(pkt.target, input_buffer + 1, len);
                    pkt.target[len] = '\0';
                    strcpy(pkt.message, space + 1);
                }
            }
        }

        int fd = open(SERVER_FIFO, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) {
            write(fd, &pkt, sizeof(pkt));
            close(fd);
        }
    }
    return NULL;
}

void* receive_thread(void *arg)
{
    Packet pkt;
    int fd = open(fifo_name, O_RDONLY);
    int history_started = 0;

    while (1) {
        ssize_t bytes = read(fd, &pkt, sizeof(pkt));

        if (bytes == 0) {
            close(fd);
            fd = open(fifo_name, O_RDONLY);
            continue;
        }

        if (bytes > 0) {
            if (strcmp(pkt.username, "HISTORY") == 0) {
                if (!history_started) {
                    printf("\n------ CHAT HISTORY ------\n");
                    history_started = 1;
                }
                printf("%s", pkt.message);
            } else {
                printf("%s: %s\n", pkt.username, pkt.message);
            }
            fflush(stdout);
        }
    }
    return NULL;
}

void handle_exit(int sig)
{
    Packet pkt = {0};
    strcpy(pkt.username, username_global);
    strcpy(pkt.message, "EXIT");
    strcpy(pkt.target, "ALL");
    pkt.type = 2;

    int fd = open(SERVER_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        write(fd, &pkt, sizeof(pkt));
        close(fd);
    }

    unlink(fifo_name);
    printf("\nExiting...\n");
    exit(0);
}

int main() {
    setbuf(stdout, NULL);

    char username[50];
    fflush(stdout);
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;

    if (strlen(username) == 0) {
        strcpy(username, "user");
    }

    strcpy(username_global, username);

    signal(SIGINT, handle_exit);

    pid_t pid = getpid();
    sprintf(fifo_name, "client_%d_fifo", pid);

    if (mkfifo(fifo_name, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo client");
    }

    // Register with server
    Packet pkt = {0};
    strcpy(pkt.username, username);
    sprintf(pkt.message, "%d %s", pid, fifo_name);
    pkt.type = 0;

    int fd = open(SERVER_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        printf("[SYSTEM]: Server not available. Please start server first.\n");
        unlink(fifo_name);
        exit(1);
    }

    write(fd, &pkt, sizeof(pkt));
    close(fd);

    pthread_t send_tid, recv_tid;
    pthread_create(&send_tid, NULL, send_thread, username);
    pthread_create(&recv_tid, NULL, receive_thread, NULL);

    pthread_join(send_tid, NULL);
    pthread_join(recv_tid, NULL);

    return 0;
}
