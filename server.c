/* server.c
Author      : Nidhi Goswami
Date        : 15-04-2026
Description : Server process for IPC Chat Application.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#define SERVER_FIFO "server_pipe"
#define BUFFER_SIZE 1024

int lock_fd;

typedef struct {
    char username[50];
    char message[256];
    char target[50];
    int type;
} Packet;

typedef struct Client {
    char username[50];
    pid_t pid;
    char fifo_name[100];
    struct Client *next;
} Client;

Client *client_head = NULL;

void add_client(char *username, pid_t pid, char *fifo)
{

    Client *temp = client_head;
    while (temp != NULL) {
        if (temp->pid == pid || strcmp(temp->username, username) == 0) {
            printf("Client already exists: %s\n", username);
            return;
        }
        temp = temp->next;
    }

    Client *new_client = (Client*)malloc(sizeof(Client));
    strcpy(new_client->username, username);
    new_client->pid = pid;
    strcpy(new_client->fifo_name, fifo);
    new_client->next = client_head;
    client_head = new_client;

    printf("Client Registered: %s | PID: %d | FIFO: %s\n", username, pid, fifo);


    FILE *fp = fopen("chat_history.txt", "r");
    if (fp != NULL) {
        char line[300];
        Packet history_pkt = {0};
        strcpy(history_pkt.username, "HISTORY");

        while (fgets(line, sizeof(line), fp)) {
            strcpy(history_pkt.message, line);
            strcpy(history_pkt.target, username);
            history_pkt.type = 1;
            Client *t = client_head;
            while (t != NULL) {
                if (strcmp(t->username, username) == 0) {
                    int fd = open(t->fifo_name, O_WRONLY | O_NONBLOCK);
                    if (fd >= 0) {
                        write(fd, &history_pkt, sizeof(Packet));
                        close(fd);
                    }
                    break;
                }
                t = t->next;
            }
        }
        fclose(fp);
    }
}

void remove_client(char *username)
{
    Client *temp = client_head;
    Client *prev = NULL;

    while (temp != NULL) {
        if (strcmp(temp->username, username) == 0) {
            if (prev == NULL)
                client_head = temp->next;
            else
                prev->next = temp->next;

            free(temp);
            printf("Client removed: %s\n", username);
            return;
        }
        prev = temp;
        temp = temp->next;
    }
}

void send_to_client(Packet *pkt)
{
    Client *temp = client_head;
    while (temp != NULL) {
        if (strcmp(temp->username, pkt->target) == 0) {
            int fd = open(temp->fifo_name, O_WRONLY | O_NONBLOCK);
            if (fd >= 0) {
                write(fd, pkt, sizeof(Packet));
                close(fd);
            }
            return;
        }
        temp = temp->next;
    }
}

void broadcast_message(Packet *pkt)
{
    FILE *fp = fopen("chat_history.txt", "a");
    if (fp != NULL) {
        fprintf(fp, "%s: %s\n", pkt->username, pkt->message);
        fclose(fp);
    }

    Client *temp = client_head;
    while (temp != NULL) {
        int fd = open(temp->fifo_name, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) {
            write(fd, pkt, sizeof(Packet));
            close(fd);
        }
        temp = temp->next;
    }
}

void handle_server_exit(int sig)
{
    printf("\n[SERVER]: Shutting down...\n");

    Packet pkt = {0};
    strcpy(pkt.username, "SYSTEM");
    strcpy(pkt.message, "Server is shutting down. Please reconnect later.");
    strcpy(pkt.target, "ALL");
    pkt.type = 1;

    Client *temp = client_head;
    while (temp != NULL) {
        int fd = open(temp->fifo_name, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) {
            write(fd, &pkt, sizeof(pkt));
            close(fd);
        }
        temp = temp->next;
    }

    close(lock_fd);
    unlink("server.lock");
    unlink(SERVER_FIFO);
    exit(0);
}

int main() {
    signal(SIGINT, handle_server_exit);

    lock_fd = open("server.lock", O_CREAT | O_RDWR, 0666);
    if (lock_fd < 0) {
        perror("Lock file open failed");
        exit(1);
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(lock_fd, F_SETLK, &lock) == -1) {
        printf("Server already running!\n");
        close(lock_fd);
        exit(1);
    }

    if (mkfifo(SERVER_FIFO, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
    }

    printf("Server started. Waiting for clients...\n");

    int fd = open(SERVER_FIFO, O_RDWR);
    if (fd < 0) {
        perror("Failed to open server FIFO");
        return 1;
    }

    Packet pkt;
    while (1) {
        ssize_t bytes = read(fd, &pkt, sizeof(pkt));

        if (bytes == 0) {
            close(fd);
            fd = open(SERVER_FIFO, O_RDWR);
            continue;
        }
        if (bytes != sizeof(pkt)) {
            continue;
        }

        if (pkt.type == 0) {
            char fifo[100];
            pid_t pid;
            sscanf(pkt.message, "%d %s", &pid, fifo);
            add_client(pkt.username, pid, fifo);
        }
        else if (pkt.type == 1) {
            if (strcmp(pkt.target, "ALL") != 0 && strlen(pkt.target) > 0) {
                send_to_client(&pkt);
            } else {
                strcpy(pkt.target, "ALL");
                broadcast_message(&pkt);
            }
        }
        else if (pkt.type == 2) {
            remove_client(pkt.username);

            Packet sys_pkt = {0};
            strcpy(sys_pkt.username, "SYSTEM");
            sprintf(sys_pkt.message, "%s left the chat", pkt.username);
            strcpy(sys_pkt.target, "ALL");
            sys_pkt.type = 1;

            broadcast_message(&sys_pkt);
        }
    }

    return 0;
}
