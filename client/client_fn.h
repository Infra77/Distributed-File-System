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

int authenticate(int sd, struct Session *session, int choice);

void list(int sd, struct Session *session);
void upload(int sd, struct Session *session, char* filename);
void download(int sd, struct Session *session, char* filename);
void update(int sd, struct Session *session, char* filename, char* filepath);
void delete(int sd, struct Session *session, char* filename);

#endif