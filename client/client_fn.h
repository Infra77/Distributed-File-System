#ifndef CLIENT_FN_H
#define CLIENT_FN_H
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

// Remote Server Commands
void *list(void* arg);
void *upload(void* arg);
void *download(void* arg);
void *update(void* arg);
void *delete(void* arg);

// Local Sandbox Commands
void *local_ls(void* arg);
void *local_touch(void* arg);
void *local_cat(void* arg);

#endif