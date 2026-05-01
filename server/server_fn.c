#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/semaphore.h>
#include <dirent.h>
#include "server_fn.h"


void handle_auth(int nsd, struct Session *session){
    int choice;
    read(nsd, &choice, sizeof(int)); // Read the authentication choice from the client

    char username[50];
    char password[50];
    char role[10];
    read(nsd, username, sizeof(username)); // Read the username from the client
    read(nsd, password, sizeof(password)); // Read the password from the client
    read(nsd, role, sizeof(role)); // Read the role from the client

    struct Session auth;
    strcpy(auth.username, username);
    strcpy(auth.password, password);
    strcpy(auth.role, role);

    if(choice==1){
        // login

        int fd=open("./server/users.txt", O_RDONLY);

        if(fd < 0){
            printf("Database issue\n");
            return;
        }

        while(read(fd, &auth, sizeof(struct Session)) > 0){
            if(strcmp(auth.username, username) == 0 && strcmp(auth.password, password) == 0 && strcmp(auth.role, role) == 0){
                int res=0;
                write(nsd, &res, sizeof(int));
                close(fd);
                return;
            }
        }
        int res=1;
        write(nsd, &res, sizeof(int));
        close(fd);
    }

    if(choice==2){
        // signup

        int fd=open("./server/users.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);

        if(fd < 0){
            printf("Database issue\n");
            return;
        }

        while(read(fd, &auth, sizeof(struct Session)) > 0){
            if(strcmp(auth.username, username) == 0){
                int res=1;
                write(nsd, &res, sizeof(int));
                close(fd);
                return;
            }
        }

        int res=0;
        write(nsd, &res, sizeof(int));
        close(fd);
    }

}
void handle_list(int nsd, struct Session *session){
    // Implement the logic to list files or resources
    char response[BUFFER_SIZE];
    
    DIR *dir;
    struct dirent *entity;
    dir = opendir("./server/files");
    if (dir == NULL) {
        printf("Error opening directory\n");
        return;
    }

    int count=0;    
    while(entity = readdir(dir)) {
        if(strcmp(entity->d_name, ".") != 0 && strcmp(entity->d_name, "..") != 0) {
            count++;
        }
    }

    write(nsd, &count, sizeof(int)); // Send the count of files

    rewinddir(dir); // Reset the directory stream to the beginning
    while(entity = readdir(dir)) {
        if(strcmp(entity->d_name, ".") != 0 && strcmp(entity->d_name, "..") != 0) {
            write(nsd, entity->d_name, sizeof(entity->d_name)); // Send each file name
        }
    }
}

void handle_download(int nsd, struct Session *session){
    char filename[200];
    char buffer[200];
    read(nsd, buffer, sizeof(buffer)); // Read the filename from the client
    strcpy(filename, buffer);

    char filepath[300];
    sprintf(filepath, "./server/files/%s", filename); // Construct the file path
    int fd=open(filepath, O_RDONLY);
    if(fd < 0){
        printf("File does not Exist in the server\n");
        return;
    }

    int filesize;
    filesize=lseek(fd, 0, SEEK_END); // Get the filesize
    lseek(fd, 0, SEEK_SET); // Reset the file pointer to the beginning
    write(nsd, &filesize, sizeof(int)); // Send the filesize to the client

    int total_bytes_sent = 0;

    while(total_bytes_sent < filesize){
        int bytes_read = read(fd, buffer, sizeof(buffer)); // Read a chunk of the file
        if(bytes_read < 0){
            printf("Dirty Read\n");
            break;
        }
        write(nsd, buffer, bytes_read); // Send the chunk to the client
        total_bytes_sent += bytes_read; // Update the total bytes sent
    }

    close(fd); // Close the file descriptor
}

void handle_upload(int nsd, struct Session *session){
    char filename[200];
    char buffer[200];
    read(nsd, buffer, sizeof(buffer)); // Read the filename from the client
    strcpy(filename, buffer);

    char filepath[300];
    sprintf(filepath, "./server/files/%s", filename); // Construct the file path
    int fd=open(filepath, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if(fd < 0){
        printf("File Already Exists on the server\n");
        return;
    }

    int filesize;
    read(nsd, &filesize, sizeof(int)); // Read the filesize from the client

    int total_bytes_received = 0;

    while(total_bytes_received < filesize){
        int bytes_read = read(nsd, buffer, sizeof(buffer)); // Read a chunk of data from the client
        if(bytes_read < 0){
            printf("Dirty Read\n");
            break;
        }
        write(fd, buffer, bytes_read); // Write the chunk to the file
        total_bytes_received += bytes_read; // Update the total bytes received
    }

    close(fd); // Close the file descriptor
}

