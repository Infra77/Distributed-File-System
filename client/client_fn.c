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


// Authentication function to handle user login or registration
int authenticate(int sd, struct Session *session, int choice){
    // update the details to the server socket descriptor
    write(sd, &choice, sizeof(choice));
    write(sd, session->username, sizeof(session->username));
    write(sd, session->password, sizeof(session->password));
    write(sd, session->role, sizeof(session->role));

    // read the authentication result from the server
    // 0 = success, 1 = failure
    int res;
    read(sd, &res, sizeof(res));
    return res;
}

// Remote Server Commands - these functions send the respective command and filename (if applicable) to the server, then read the response and display the results to the user. Each command is executed in a separate thread to allow for concurrent operations without blocking the main command loop. The client uses TCP protocol to communicate with the server, sending commands and receiving responses in a structured format.

// list command to list all files on the server.
void *list(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;
    
    char cmd[200];
    memset(cmd, 0, sizeof(cmd)); 
    strcpy(cmd, "list");
    // send command to server
    write(sd, cmd, sizeof(cmd));
    
    int count;
    // read the number of files from the server
    read(sd, &count, sizeof(count));
    printf("\n--- files on server (%d) ---\n", count);

    char filename[200];
    char author[50];
    int  is_deleted;
    for(int i = 0; i < count; i++){
        // read file details from the server
        read(sd, filename, 200);
        read(sd, author, 50);
        read(sd, &is_deleted, sizeof(int));
        printf("  %d. %-20s [author: %s]\n", i+1, filename, author);
    }
    printf("----------------------------\n");

    return NULL;
}


// upload command to upload a file to the server - the uploader becomes the author of the file.
void* upload(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;
    
    // Construct the local file path based on the user's directory and filename
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
    // send command and filename to server
    write(sd, cmd, sizeof(cmd));
    write(sd, thread_args->filename, sizeof(thread_args->filename));
    
    int res;
    read(sd, &res, sizeof(int));
    if(res == 1){ 
        printf("File already exists on server.\n");
        close(fd);
        return NULL; 
    }

    // Get the file size and send it to the server
    int filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    write(sd, &filesize, sizeof(int));

    char buffer[BUFFER_SIZE];
    // Read the file in chunks and send it to the server - TCP protocol.
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


// download command to download a file from the server.
void *download(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;
    
    char cmd[200];
    memset(cmd, 0, sizeof(cmd));
    strcpy(cmd, "download");
    // send command and filename to server
    write(sd, cmd, sizeof(cmd));
    write(sd, thread_args->filename, sizeof(thread_args->filename));

    int res;
    read(sd, &res, sizeof(int));
    if(res != 0){
        printf("File not found on server.\n");
        return NULL;
    }

    // read the file size from the server
    int filesize;
    read(sd, &filesize, sizeof(int));

    char buffer[BUFFER_SIZE];
    int total_bytes_received = 0;

    // Construct the local file path based on the user's directory and filename
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

    // Read the file data from the server in chunks and write it to the local file - TCP protocol.
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


// update command to update an existing file on the server - only allowed for Admins and Authors.
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
    // send command and filename to server
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
    // Check if the user has permission to update the file - only Admins and Authors can update.
    if(res == 2){ 
        printf("Permission denied - Admin or Author access required.\n"); 
        close(fd);
        return NULL; 
    }

    // Get the file size and send it to the server
    int filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    write(sd, &filesize, sizeof(int));

    char buffer[BUFFER_SIZE];
    // Read the file in chunks and send it to the server - TCP protocol.
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


// delete command to delete a file from the server - only allowed for Admins and Authors.
void *delete(void *arg) {
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    int sd = thread_args->sd;

    char cmd[200];
    memset(cmd, 0, sizeof(cmd));
    // send command and filename to server
    strcpy(cmd, "delete");
    write(sd, cmd, sizeof(cmd));
    write(sd, thread_args->filename, sizeof(thread_args->filename));

    int res;
    read(sd, &res, sizeof(int));
    if(res == 1){
        printf("File not found on server.\n");
    } 
    // Check if the user has permission to delete the file - only Admins and Authors can delete.
    else if(res == 2){
        printf("Permission denied - Admin or Author access required.\n");
    } 
    else {
        printf("Delete successful: %s\n", thread_args->filename);
    }

    return NULL;
}

// Local sandbox commands to execute basic file operations in the user's local directory - ls, touch, cat using child processes and execvp. These commands do not interact with the server and operate only on the client's local filesystem. Each user has their own local directory named after their username where these commands are executed to ensure isolation between users.


// local_ls command to list files in the user's local directory.
void *local_ls(void *arg) {
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    // Construct the user directory path based on the username
    char userdir[200];
    snprintf(userdir, sizeof(userdir), "./%s", thread_args->session.username);

    // fork a child process to execute the ls command in the user's local directory using execvp executing "ls" command. The parent process waits for the child to complete before returning to the main command loop.
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


// local_touch command to create a new file in the user's local directory.
void *local_touch(void *arg) {
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;

    // Construct the user directory path based on the username
    char userdir[200];
    snprintf(userdir, sizeof(userdir), "./%s", thread_args->session.username);

    if (strlen(thread_args->filename) == 0) {
        printf("Usage: touch <filename>\n");
        return NULL;
    }

    // fork a child process to execute the touch command in the user's local directory using execvp executing "touch" command. The parent process waits for the child to complete before returning to the main command loop.
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


// local_cat command to display the contents of a file in the user's local directory.
void *local_cat(void *arg) {
    struct Thread_Args *thread_args = (struct Thread_Args*)arg;
    // Construct the user directory path based on the username
    char userdir[200];
    snprintf(userdir, sizeof(userdir), "./%s", thread_args->session.username);

    if (strlen(thread_args->filename) == 0) {
        printf("Usage: cat <filename>\n");
        return NULL;
    }

    // fork a child process to execute the cat command in the user's local directory using execvp executing "cat" command. The parent process waits for the child to complete before returning to the main command loop.
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