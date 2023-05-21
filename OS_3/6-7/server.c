#include <stdio.h>     
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>   
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAXPENDING 5
#define FLOWERS_COUNT 40

pthread_mutex_t mutex;
int flowers[FLOWERS_COUNT];
int flowers_count = 0;
int observer_client = -1;

typedef struct thread_args {
    int socket;
} thread_args;

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

void handleFlowers(int socket) {
    int buffer[3];
    for (;;) {
        recv(socket, buffer, sizeof(buffer), 0);
        pthread_mutex_lock(&mutex);
        int flower_index = buffer[0];
        printf("FLower #%d is dying\n", flower_index);
        if (observer_client > 0) {
            char msg[32];
            snprintf(msg, sizeof(msg), "FLower #%d is dying\n", flower_index);
            send(observer_client, msg, sizeof(msg), 0);
        }
        flowers[flowers_count++] = flower_index;
        sleep(2);
        buffer[1] = flower_index;
        pthread_mutex_unlock(&mutex);
        send(socket, buffer, sizeof(buffer), 0);
        sleep(1);
        if (observer_client > 0) {
            char msg[32];
            snprintf(msg, sizeof(msg), "Flower #%d is alive\n", flower_index);
            send(observer_client, msg, sizeof(msg), 0);
        }
    }
}

void *handleClient(void *args) {
    int socket;
    pthread_detach(pthread_self());
    socket = ((thread_args*)args)->socket;
    free(args);
    int buffer[3];
    for (;;) {
        recv(socket, buffer, sizeof(buffer), 0);
        pthread_mutex_lock(&mutex);
        int client_index = buffer[0];
        if (flowers_count > 0) {
            int fl = flowers[--flowers_count];
            printf("Giving flower #%d to #%d\n", fl, client_index);
            if (observer_client > 0) {
                char msg[32];
                snprintf(msg, sizeof(msg), "Giving flower #%d to #%d\n", fl, client_index);
                send(observer_client, msg, sizeof(msg), 0);
            }
            buffer[1] = fl;
        } else {
            buffer[1] = -1;
        }
        pthread_mutex_unlock(&mutex);
        send(socket, buffer, sizeof(buffer), 0);
    }
    close(socket);
}

void *clientThread(void *args) {
    int server_socket;
    int client_socket;
    int client_length;
    pthread_t threadId;
    struct sockaddr_in client_addr;
    pthread_detach(pthread_self());
    server_socket = ((thread_args*)args)->socket;
    free(args);
    listen(server_socket, MAXPENDING);
    for (;;) {
        client_length = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, &client_length);
        printf("New connection from %s\n", inet_ntoa(client_addr.sin_addr));

        thread_args *args = (thread_args*) malloc(sizeof(thread_args));
        args->socket = client_socket;
        if (pthread_create(&threadId, NULL, handleClient, (void*) args) != 0) DieWithError("pthread_create() failed");
    }
}

void *flowersThread(void *args) {
    int server_socket;
    int client_socket;
    int client_length;
    pthread_t threadId;
    struct sockaddr_in client_addr;
    pthread_detach(pthread_self());
    server_socket = ((thread_args*)args)->socket;
    free(args);
    listen(server_socket, MAXPENDING);
    for (;;) {
        client_length = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, &client_length);
        printf("New connection from %s\n", inet_ntoa(client_addr.sin_addr));
        handleFlowers(client_socket);
    }
}

int createTCPSocket(unsigned short server_port) {
    int server_socket;
    struct sockaddr_in server_addr;

    if ((server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) DieWithError("socket() failed");
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;              
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons(server_port);

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) DieWithError("bind() failed");
    printf("Open socket on %s:%d\n", inet_ntoa(server_addr.sin_addr), server_port);
    return server_socket;
}

int main(int argc, char *argv[])
{
    unsigned short server_port;
    unsigned short flowers_port;
    unsigned short observer_port;
    int server_socket;
    int flowers_socket;
    int observer_socket;
    int client_socket;
    int client_length;
    struct sockaddr_in client_addr;
    pthread_t threadId;
    pthread_mutex_init(&mutex, NULL);
    if (argc != 4)
    {
        fprintf(stderr, "Usage:  %s <Port for clients> <Port for flowers> <Port for observer>\n", argv[0]);
        exit(1);
    }

    server_port = atoi(argv[1]);
    flowers_port = atoi(argv[2]);
    observer_port = atoi(argv[3]);

    server_socket = createTCPSocket(server_port);
    flowers_socket = createTCPSocket(flowers_port);
    observer_socket = createTCPSocket(observer_port);


    thread_args *args = (thread_args*) malloc(sizeof(thread_args));
    args->socket = server_socket;
    if (pthread_create(&threadId, NULL, clientThread, (void*) args) != 0) DieWithError("pthread_create() failed");

    thread_args *args1 = (thread_args*) malloc(sizeof(thread_args));
    args1->socket = flowers_socket;
    if (pthread_create(&threadId, NULL, flowersThread, (void*) args1) != 0) DieWithError("pthread_create() failed");

    listen(observer_socket, MAXPENDING);
    for (;;) {
        client_length = sizeof(client_addr);
        observer_client = accept(observer_socket, (struct sockaddr *) &client_addr, &client_length);
        printf("New connection from %s\n", inet_ntoa(client_addr.sin_addr));
        for (;;) {
            sleep(1);
        }
        close(observer_client);
    }
    pthread_mutex_destroy(&mutex);
    return 0;
}