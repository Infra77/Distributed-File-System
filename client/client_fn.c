#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <mqueue.h>
#include "client_fn.h"

// authenticate
// sends choice, username, password, role
// receives 0 for success, 1 for failure

int authenticate(int sd, struct Session *session, int choice){
    write(sd, &choice, sizeof(choice));
    write(sd, session->username, sizeof(session->username));
    write(sd, session->password, sizeof(session->password));
    write(sd, session->role, sizeof(session->role));

    int res;
    read(sd, &res, sizeof(res));
    return res;
}

// list
void *list(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;
    char cmd[200];
    strcpy(cmd, "list");
    write(sd, cmd, sizeof(cmd));
    
    int count;
    read(sd, &count, sizeof(count));
    printf("\n--- files on server (%d) ---\n", count);

    char filename[200];
    char author[50];
    int  is_deleted;
    for(int i = 0; i < count; i++){
        read(sd, filename, 200);
        read(sd, author, 50);
        read(sd, &is_deleted, sizeof(int));
        printf("  %d. %s  [author: %s]\n", i+1, filename, author);
    }
    printf("----------------------------\n");

    return NULL;
}

void* upload(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;
    char cmd[200];
    
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "./client/%s/%s", thread_args->session.username, thread_args->filename);
    
    int fd = open(filepath, O_RDONLY);
    if(fd < 0){
        printf("File not found locally\n");
    
        return NULL;
    }

    strcpy(cmd, "upload");
    write(sd, cmd, sizeof(cmd));

    write(sd, thread_args->filename, sizeof(thread_args->filename));
    
    int res;
    read(sd, &res, sizeof(int));
    if(res == 1){ 
        printf("File already exists on server\n");
     
        return NULL; 
    }
    if(res == 2){ 
        printf("permission denied - not the author\n"); 
     
        return NULL; 
    }

    int filesize=lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    write(sd, &filesize, sizeof(int));

    char buffer[BUFFER_SIZE];
    int total_bytes_sent = 0;
    while(total_bytes_sent < filesize){
        int n = read(fd, buffer, sizeof(buffer));
        if(n <= 0) break;
        write(sd, buffer, n);
        total_bytes_sent += n;
    }

    printf("Upload completed: %s\n", thread_args->filename);
    close(fd);


    return NULL;
}

void *download(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;
    char cmd[200];
    strcpy(cmd, "download");
    write(sd, cmd, sizeof(cmd));
    write(sd, thread_args->filename, sizeof(thread_args->filename));

    int res;
    read(sd, &res, sizeof(int));
    if(res!=0){
        printf("File not found on server\n");
    
        return NULL;
    }

    int filesize;
    read(sd, &filesize, sizeof(int));

    char buffer[BUFFER_SIZE];
    int total_bytes_received = 0;
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "./client/%s/%s", thread_args->session.username, thread_args->filename);
    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    while(total_bytes_received < filesize){
        int n = read(sd, buffer, sizeof(buffer));
        if(n <= 0) break;
        write(fd, buffer, n);
        total_bytes_received += n;
    }

    printf("Download completed: %s\n", thread_args->filename);

    close(fd);

    return NULL;
}

void *update(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;
    char cmd[200];
    
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "./client/%s/%s", thread_args->session.username, thread_args->filename);
    
    int fd = open(filepath, O_RDONLY);
    if(fd < 0){
        printf("File not found locally\n");
    
        return NULL;
    }

    strcpy(cmd, "update");
    write(sd, cmd, sizeof(cmd));

    write(sd, thread_args->filename, sizeof(thread_args->filename));
    
    int res;
    read(sd, &res, sizeof(int));
    if(res == 1){ 
        printf("File not found on server\n");
     
        return NULL; 
    }
    if(res == 2){ 
        printf("permission denied - not the author\n"); 
     
        return NULL; 
    }

    int filesize=lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    write(sd, &filesize, sizeof(int));

    char buffer[BUFFER_SIZE];
    int total_bytes_sent = 0;
    while(total_bytes_sent < filesize){
        int n = read(fd, buffer, sizeof(buffer));
        if(n <= 0) break;
        write(sd, buffer, n);
        total_bytes_sent += n;
    }

    printf("Update completed: %s\n", thread_args->filename);

    close(fd);


    return NULL;
}

// delete
// sends filename, receives 0=ok 1=not found 2=not admin
void *delete(void *arg) {
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;

    char cmd[200];
    strcpy(cmd, "delete");
    write(sd, cmd, sizeof(cmd));

    write(sd, thread_args->filename, sizeof(thread_args->filename));

    int res;
    read(sd, &res, sizeof(int));
    if(res == 1){
        printf("File not found on server\n");
    } 
    else if(res == 2){
        printf("Permission denied - not an admin\n");
    } 
    else {
        printf("Delete successful: %s\n", thread_args->filename);
    }


    return NULL;
}