#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/semaphore.h>

struct Session{
    char username[50];
    char password[50];
    int role[10];
};

void handle_list(int nsd, struct Session *session);
void handle_download(int nsd, struct Session *session);
void handle_upload(int nsd, struct Session *session);
void handle_update(int nsd, struct Session *session);