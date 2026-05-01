#ifndef SERVER_FN_H
#define SERVER_FN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

extern sem_t download_sem;

struct Session{
    char username[50];
    char password[50];
    char role[10];
};

struct Meta{
    char filename[200];
    char author[50];
    int is_deleted;
};

struct Thread_Args{
    int nsd;
    struct Session session;
};

int handle_auth(int nsd, struct Session *session);
void *handle_list(void* arg);
void *handle_upload(void* arg);
void *handle_download(void* arg);
void *handle_update(void* arg);
void *handle_delete(void* arg);

#endif