#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "client_fn.h"

int authenticate(int sd, struct Session *session, int choice){
    write(sd, &choice, sizeof(choice));
    write(sd, session->username, sizeof(session->username));
    write(sd, session->password, sizeof(session->password));
    write(sd, session->role, sizeof(session->role));

    int res;
    read(sd, &res, sizeof(res));
    return res;
}

void *list(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;
    
    char cmd[200];
    memset(cmd, 0, sizeof(cmd)); 
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
        printf("  %d. %-20s [author: %s]\n", i+1, filename, author);
    }
    printf("----------------------------\n");

    return NULL;
}

void* upload(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;
    
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "./%s/%s", thread_args->session.username, thread_args->filename);
    
    int fd = open(filepath, O_RDONLY);
    if(fd < 0){
        printf("File not found locally in %s's directory.\n", thread_args->session.username);
        return NULL;
    }

    char cmd[200];
    memset(cmd, 0, sizeof(cmd));
    strcpy(cmd, "upload");
    write(sd, cmd, sizeof(cmd));
    write(sd, thread_args->filename, sizeof(thread_args->filename));
    
    int res;
    read(sd, &res, sizeof(int));
    if(res == 1){ 
        printf("File already exists on server.\n");
        close(fd);
        return NULL; 
    }

    int filesize = lseek(fd, 0, SEEK_END);
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
    memset(cmd, 0, sizeof(cmd));
    strcpy(cmd, "download");
    write(sd, cmd, sizeof(cmd));
    write(sd, thread_args->filename, sizeof(thread_args->filename));

    int res;
    read(sd, &res, sizeof(int));
    if(res != 0){
        printf("File not found on server.\n");
        return NULL;
    }

    int filesize;
    read(sd, &filesize, sizeof(int));

    char buffer[BUFFER_SIZE];
    int total_bytes_received = 0;
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "./%s/%s", thread_args->session.username, thread_args->filename);
    
    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("Failed to create local file.\n");
        while(total_bytes_received < filesize) {
            int n = read(sd, buffer, sizeof(buffer));
            if(n <= 0) break;
            total_bytes_received += n;
        }
        return NULL;
    }

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
    
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "./%s/%s", thread_args->session.username, thread_args->filename);
    
    int fd = open(filepath, O_RDONLY);
    if(fd < 0){
        printf("File not found locally in %s's directory.\n", thread_args->session.username);
        return NULL;
    }

    char cmd[200];
    memset(cmd, 0, sizeof(cmd));
    strcpy(cmd, "update");
    write(sd, cmd, sizeof(cmd));
    write(sd, thread_args->filename, sizeof(thread_args->filename));
    
    int res;
    read(sd, &res, sizeof(int));
    if(res == 1){ 
        printf("File not found on server.\n");
        close(fd);
        return NULL; 
    }
    if(res == 2){ 
        printf("Permission denied - Admin or Author access required.\n"); 
        close(fd);
        return NULL; 
    }

    int filesize = lseek(fd, 0, SEEK_END);
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

void *delete(void *arg) {
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;

    char cmd[200];
    memset(cmd, 0, sizeof(cmd));
    strcpy(cmd, "delete");
    write(sd, cmd, sizeof(cmd));
    write(sd, thread_args->filename, sizeof(thread_args->filename));

    int res;
    read(sd, &res, sizeof(int));
    if(res == 1){
        printf("File not found on server.\n");
    } 
    else if(res == 2){
        printf("Permission denied - Admin or Author access required.\n");
    } 
    else {
        printf("Delete successful: %s\n", thread_args->filename);
    }

    return NULL;
}

void *local_ls(void *arg) {
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    char userdir[200];
    snprintf(userdir, sizeof(userdir), "./%s", thread_args->session.username);

    pid_t pid = fork();
    if (pid == 0) {
        chdir(userdir);
        execlp("ls", "ls", NULL);
        perror("ls failed");
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    }
    return NULL;
}

void *local_touch(void *arg) {
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    char userdir[200];
    snprintf(userdir, sizeof(userdir), "./%s", thread_args->session.username);

    if (strlen(thread_args->filename) == 0) {
        printf("Usage: touch <filename>\n");
        return NULL;
    }

    pid_t pid = fork();
    if (pid == 0) {
        chdir(userdir);
        execlp("touch", "touch", thread_args->filename, NULL);
        perror("touch failed");
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    }
    return NULL;
}

void *local_cat(void *arg) {
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    char userdir[200];
    snprintf(userdir, sizeof(userdir), "./%s", thread_args->session.username);

    if (strlen(thread_args->filename) == 0) {
        printf("Usage: cat <filename>\n");
        return NULL;
    }

    pid_t pid = fork();
    if (pid == 0) {
        chdir(userdir);
        execlp("cat", "cat", thread_args->filename, NULL);
        perror("cat failed");
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    }
    return NULL;
}