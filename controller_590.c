#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9410

#define SID "0952"
#define AUTH_TOKEN "OPS-2590"

#define MAX_LINE 8192
#define BUFFER_SIZE 8192

static int udp_socket_fd = -1;
static volatile int udp_running = 0;
static pthread_t udp_thread;

/* ---------------- SEND ALL ---------------- */

int send_all(int fd, const void *data, size_t length)
{
    size_t sent = 0;
    const char *buffer = (const char *)data;

    while (sent < length) {

        ssize_t n = send(fd,
                         buffer + sent,
                         length - sent,
                         0);

        if (n <= 0) {

            if (errno == EINTR)
                continue;

            return -1;
        }

        sent += (size_t)n;
    }

    return 0;
}

/* ---------------- SEND LINE ---------------- */

int send_line(int fd, const char *line)
{
    size_t len = strlen(line);

    if (send_all(fd, line, len) < 0)
        return -1;

    if (len == 0 || line[len - 1] != '\n') {

        if (send_all(fd, "\n", 1) < 0)
            return -1;
    }

    return 0;
}

/* ---------------- RECEIVE LINE ---------------- */

int recv_line(int fd, char *buffer, size_t size)
{
    size_t pos = 0;

    while (pos < size - 1) {

        char c;

        ssize_t n = recv(fd, &c, 1, 0);

        if (n == 0) {

            if (pos == 0)
                return 0;

            break;
        }

        if (n < 0) {

            if (errno == EINTR)
                continue;

            return -1;
        }

        if (c == '\n')
            break;

        if (c != '\r')
            buffer[pos++] = c;
    }

    buffer[pos] = '\0';

    return 1;
}

/* ---------------- RECEIVE EXACT BYTES ---------------- */

int recv_exact(int fd, void *data, size_t length)
{
    size_t received = 0;
    char *buffer = (char *)data;

    while (received < length) {

        ssize_t n = recv(fd,
                         buffer + received,
                         length - received,
                         0);

        if (n == 0)
            return -1;

        if (n < 0) {

            if (errno == EINTR)
                continue;

            return -1;
        }

        received += (size_t)n;
    }

    return 0;
}

/* ---------------- UDP MONITOR THREAD ---------------- */

void *udp_listener(void *arg)
{
    (void)arg;

    char buffer[1024];

    while (udp_running) {

        fd_set readfds;

        FD_ZERO(&readfds);
        FD_SET(udp_socket_fd, &readfds);

        struct timeval timeout;

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int result =
            select(udp_socket_fd + 1,
                   &readfds,
                   NULL,
                   NULL,
                   &timeout);

        if (result <= 0)
            continue;

        if (FD_ISSET(udp_socket_fd, &readfds)) {

            ssize_t n =
                recvfrom(udp_socket_fd,
                         buffer,
                         sizeof(buffer) - 1,
                         0,
                         NULL,
                         NULL);

            if (n > 0) {

                buffer[n] = '\0';

                printf("\n[UDP MONITOR] %s\n",
                       buffer);

                printf("Enter command: ");
                fflush(stdout);
            }
        }
    }

    return NULL;
}

/* ---------------- PUT FILE ---------------- */

void put_file(int sock)
{
    char filename[256];

    printf("Enter local filename: ");
    fflush(stdout);

    if (scanf("%255s", filename) != 1)
        return;

    FILE *fp = fopen(filename, "rb");

    if (fp == NULL) {

        perror("Cannot open local file");
        return;
    }

    fseek(fp, 0, SEEK_END);

    long long filesize =
        (long long)ftell(fp);

    fseek(fp, 0, SEEK_SET);

    printf("File size: %lld bytes\n",
           filesize);

    char command[512];

    snprintf(command,
             sizeof(command),
             "PUT %s %lld\n",
             filename,
             filesize);

    if (send_all(sock,
                 command,
                 strlen(command)) < 0) {

        fclose(fp);
        return;
    }

    char buffer[BUFFER_SIZE];

    long long remaining = filesize;

    while (remaining > 0) {

        size_t wanted =
            remaining > BUFFER_SIZE
            ? BUFFER_SIZE
            : (size_t)remaining;

        size_t n =
            fread(buffer, 1, wanted, fp);

        if (n == 0)
            break;

        if (send_all(sock,
                     buffer,
                     n) < 0) {

            fclose(fp);
            return;
        }

        remaining -= (long long)n;
    }

    fclose(fp);

    char response[MAX_LINE];

    if (recv_line(sock,
                  response,
                  sizeof(response)) > 0) {

        printf("Server: %s\n",
               response);
    }
}

/* ---------------- GET FILE ---------------- */

void get_file(int sock)
{
    char filename[256];

    printf("Enter remote filename: ");
    fflush(stdout);

    if (scanf("%255s", filename) != 1)
        return;

    char command[512];

    snprintf(command,
             sizeof(command),
             "GET %s\n",
             filename);

    if (send_all(sock,
                 command,
                 strlen(command)) < 0)
        return;

    char response[MAX_LINE];

    if (recv_line(sock,
                  response,
                  sizeof(response)) <= 0)
        return;

    printf("Server: %s\n",
           response);

    long long filesize;

    char received_name[256];

    if (sscanf(response,
               "OK FILE_SEND %255s %lld SID:%*s",
               received_name,
               &filesize) != 2) {

        return;
    }

    char output_name[512];

    snprintf(output_name,
             sizeof(output_name),
             "received_%s",
             received_name);

    FILE *fp =
        fopen(output_name, "wb");

    if (fp == NULL) {

        perror("Cannot create received file");
        return;
    }

    char buffer[BUFFER_SIZE];

    long long remaining = filesize;

    while (remaining > 0) {

        size_t wanted =
            remaining > BUFFER_SIZE
            ? BUFFER_SIZE
            : (size_t)remaining;

        if (recv_exact(sock,
                       buffer,
                       wanted) < 0) {

            fclose(fp);
            return;
        }

        fwrite(buffer,
               1,
               wanted,
               fp);

        remaining -= (long long)wanted;
    }

    fclose(fp);

    printf("Downloaded successfully: %s\n",
           output_name);
}

/* ---------------- CONNECT ---------------- */

int connect_server(void)
{
    int sock =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (sock < 0) {

        perror("socket");
        return -1;
    }

    struct sockaddr_in server;

    memset(&server,
           0,
           sizeof(server));

    server.sin_family =
        AF_INET;

    server.sin_port =
        htons(SERVER_PORT);

    inet_pton(AF_INET,
              SERVER_IP,
              &server.sin_addr);

    if (connect(sock,
                (struct sockaddr *)&server,
                sizeof(server)) < 0) {

        perror("connect");

        close(sock);

        return -1;
    }

    return sock;
}

/* ---------------- BAD AUTH TEST ---------------- */

void bad_auth_test(void)
{
    int sock = connect_server();

    if (sock < 0)
        return;

    send_line(sock,
              "AUTH WRONG-TOKEN");

    char response[MAX_LINE];

    if (recv_line(sock,
                  response,
                  sizeof(response)) > 0) {

        printf("Server: %s\n",
               response);
    }

    close(sock);
}

/* ---------------- MAIN ---------------- */

int main(int argc, char *argv[])
{
    if (argc > 1 &&
        strcmp(argv[1], "bad") == 0) {

        printf("Testing incorrect authentication...\n");

        bad_auth_test();

        return 0;
    }

    int sock = connect_server();

    if (sock < 0)
        return 1;

    printf("\n====================================\n");
    printf("       RemoteOps Controller\n");
    printf("====================================\n");
    printf("Server : %s\n", SERVER_IP);
    printf("Port   : %d\n", SERVER_PORT);
    printf("SID    : %s\n", SID);
    printf("====================================\n");

    /* AUTH */

    send_line(sock,
              "AUTH " AUTH_TOKEN);

    char response[MAX_LINE];

    if (recv_line(sock,
                  response,
                  sizeof(response)) <= 0) {

        printf("Authentication response not received.\n");

        close(sock);

        return 1;
    }

    printf("Server: %s\n",
           response);

    if (strncmp(response,
                "OK AUTHENTICATED",
                16) != 0) {

        printf("Authentication failed.\n");

        close(sock);

        return 1;
    }

    while (1) {

        printf("\n");
        printf("========== RemoteOps ==========\n");
        printf("1. SYSINFO\n");
        printf("2. LISTPROC\n");
        printf("3. EXEC command\n");
        printf("4. PUT file\n");
        printf("5. GET file\n");
        printf("6. MONITOR START\n");
        printf("7. MONITOR STOP\n");
        printf("8. QUIT\n");
        printf("===============================\n");

        printf("Enter command: ");
        fflush(stdout);

        int choice;

        if (scanf("%d", &choice) != 1)
            break;

        if (choice == 1) {

            send_line(sock,
                      "SYSINFO");

            if (recv_line(sock,
                          response,
                          sizeof(response)) > 0) {

                printf("Server: %s\n",
                       response);
            }
        }

        else if (choice == 2) {

            send_line(sock,
                      "LISTPROC");

            if (recv_line(sock,
                          response,
                          sizeof(response)) > 0) {

                printf("Server:\n%s\n",
                       response);
            }
        }

        else if (choice == 3) {

            char command[256];

            printf("Enter EXEC command ");
            printf("(DATE/UPTIME/DISKFREE/HOSTNAME/WHOAMI): ");

            scanf("%255s", command);

            char request[512];

            snprintf(request,
                     sizeof(request),
                     "EXEC %s",
                     command);

            send_line(sock,
                      request);

            if (recv_line(sock,
                          response,
                          sizeof(response)) > 0) {

                printf("Server: %s\n",
                       response);
            }
        }

        else if (choice == 4) {

            put_file(sock);
        }

        else if (choice == 5) {

            get_file(sock);
        }

        else if (choice == 6) {

            int port;

            printf("Enter UDP port (example 9000): ");

            scanf("%d", &port);

            if (port < 1 ||
                port > 65535) {

                printf("Invalid UDP port.\n");
                continue;
            }

            udp_socket_fd =
                socket(AF_INET,
                       SOCK_DGRAM,
                       0);

            if (udp_socket_fd < 0) {

                perror("UDP socket");
                continue;
            }

            struct sockaddr_in local;

            memset(&local,
                   0,
                   sizeof(local));

            local.sin_family =
                AF_INET;

            local.sin_addr.s_addr =
                INADDR_ANY;

            local.sin_port =
                htons(port);

            if (bind(udp_socket_fd,
                     (struct sockaddr *)&local,
                     sizeof(local)) < 0) {

                perror("UDP bind");

                close(udp_socket_fd);
                udp_socket_fd = -1;

                continue;
            }

            udp_running = 1;

            if (pthread_create(&udp_thread,
                               NULL,
                               udp_listener,
                               NULL) != 0) {

                printf("Could not create UDP listener.\n");

                udp_running = 0;

                close(udp_socket_fd);
                udp_socket_fd = -1;

                continue;
            }

            char request[256];

            snprintf(request,
                     sizeof(request),
                     "MONITOR START %d",
                     port);

            send_line(sock,
                      request);

            if (recv_line(sock,
                          response,
                          sizeof(response)) > 0) {

                printf("Server: %s\n",
                       response);
            }
        }

        else if (choice == 7) {

            send_line(sock,
                      "MONITOR STOP");

            if (recv_line(sock,
                          response,
                          sizeof(response)) > 0) {

                printf("Server: %s\n",
                       response);
            }

            if (udp_running) {

                udp_running = 0;

                pthread_join(udp_thread,
                             NULL);

                close(udp_socket_fd);

                udp_socket_fd = -1;
            }
        }

        else if (choice == 8) {

            send_line(sock,
                      "QUIT");

            if (recv_line(sock,
                          response,
                          sizeof(response)) > 0) {

                printf("Server: %s\n",
                       response);
            }

            break;
        }

        else {

            printf("Invalid option.\n");
        }
    }

    if (udp_running) {

        udp_running = 0;

        pthread_join(udp_thread,
                     NULL);

        close(udp_socket_fd);
    }

    close(sock);

    printf("\nController closed.\n");

    return 0;
}
