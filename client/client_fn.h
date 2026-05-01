#ifndef CLIENT_FN_H
#define CLIENT_FN_H
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mqueue.h>

#define BUFFER_SIZE 1000

struct Session{
    char username[50];
    char password[50];
    char role[10];
};

struct Thread_Args{
    int sd;
    struct Session session;
    char filename[200];
};

int authenticate(int sd, struct Session *session, int choice);

void *list(void* arg);
void *upload(void* arg);
void *download(void* arg);
void *update(void* arg);
void *delete(void* arg);

#endif