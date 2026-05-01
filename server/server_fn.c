#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>
#include <fcntl.h>
#include <dirent.h>
#include "server_fn.h"


#define PORT 8080
#define BUFFER_SIZE 1000
#define USER_FILES "./server/users.txt"
#define METADATA_FILES "./server/metadata.txt"
#define FILES_DIR "./server/files/"

sem_t download_sem;




// log message queue function



// Authorization function
// Client sends choice (1 for login, 2 for signup) and username[50], password[50], role[10]
// Server sends 0 for success and 1 for failure
int handle_auth(int nsd, struct Session *session){
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

        int fd=open(USER_FILES, O_RDONLY);

        if(fd < 0){
            printf("Database issue\n");
            return 1;
        }

        struct Session rec;
        while(read(fd, &rec, sizeof(struct Session)) > 0){
            if(strcmp(auth.username, rec.username) == 0 && strcmp(auth.password, rec.password) == 0 && strcmp(auth.role, rec.role) == 0){
                int res=0;
                write(nsd, &res, sizeof(int));
                close(fd);
                strcpy(session->username, auth.username);
                strcpy(session->password, auth.password);
                strcpy(session->role, auth.role);
                return 0;
            }
        }
        int res=1;
        write(nsd, &res, sizeof(int));
        close(fd);
        
        return 1;
    }

    if(choice==2){
        // signup

        int fd = open(USER_FILES, O_RDONLY);
        if (fd >= 0) {
            struct Session rec;
            while (read(fd, &rec, sizeof(struct Session)) > 0) {
                if (strcmp(rec.username, auth.username) == 0) {
                    close(fd);
                    int res=1; 
                    write(nsd, &res, sizeof(int));
                    return 1;
                }
            }
            close(fd);
        }
        fd=open(USER_FILES, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(fd < 0){
            printf("Database issue\n");
            return 1;
        }
        write(fd, &auth, sizeof(struct Session)); // Write the new user session to the file
        int res=0;
        write(nsd, &res, sizeof(int));
        close(fd);

        strcpy(session->username, auth.username);
        strcpy(session->password, auth.password);
        strcpy(session->role, auth.role);

        return 0;
    }
}



// list function
// shows all files which have not been deleted
void *handle_list(void* arg){
    // Implement the logic to list files or resources
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;

    int nsd= thread_args->nsd;
    
    int fd=open(METADATA_FILES, O_RDONLY);
    if(fd < 0){
        printf("Database issue\n");
        int count=0;
        write(nsd, &count, sizeof(int));
        free(thread_args);
        return NULL;
    }

    struct Meta rec;
    int count=0;
    while(read(fd, &rec, sizeof(struct Meta)) > 0){
        if(rec.is_deleted == 0){
            count++;
        }
    }
    write(nsd, &count, sizeof(int)); // Send the count of non-deleted files to the client
    lseek(fd, 0, SEEK_SET); // Reset the file pointer to the beginning

    while(read(fd, &rec, sizeof(struct Meta)) > 0){
        if(rec.is_deleted == 0){
            write(nsd, rec.filename, sizeof(rec.filename));
            write(nsd, rec.author, sizeof(rec.author));
            write(nsd, &rec.is_deleted, sizeof(int)); // Send the metadata of each non-deleted file to the client
        }
    }
    close(fd);

    printf("List command handled for %s\n", thread_args->session.username);
    free(thread_args);
    return NULL;
}



// upload function
// flock the destination file so that we can only permit 1 upload at a time
// sleep(1)+sleep(4) to show concurrency
void *handle_upload(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;
    int nsd= thread_args->nsd;

    char filename[200];
    read(nsd, filename, sizeof(filename)); // Read the filename from the client

    sleep(1); // Simulate some processing time
    int mfd=open(METADATA_FILES, O_RDONLY);
    if(mfd>=0){
        struct Meta rec;
        while(read(mfd, &rec, sizeof(struct Meta)) > 0){
            if(strcmp(rec.filename, filename) == 0 && rec.is_deleted == 0){
                printf("File with the same name already exists\n");
                int res=1;
                write(nsd, &res, sizeof(int));
                close(mfd);
                free(thread_args);
                return NULL;
            }
        }
        close(mfd);
    }

    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s/%s", FILES_DIR, filename); // Construct the file path

    int fd=open(filepath, O_WRONLY | O_CREAT | O_EXCL, 0666); // Open the file for writing, create it if it doesn't exist, and fail if it already exists
    if(fd<0){
        printf("Database issue\n");
        int res=1;
        write(nsd, &res, sizeof(int));
        free(thread_args);
        return NULL;
    }

    struct flock lock;
    lock.l_type = F_WRLCK; // Write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    fcntl(fd, F_SETLKW, &lock); // Acquire the lock - blocks if another upload is in progress

    int res=0;
    write(nsd, &res, sizeof(int)); // Send success response to the client

    int filesize;
    read(nsd, &filesize, sizeof(int)); // Read the filesize from the client

    char buffer[BUFFER_SIZE];
    int total_bytes_received = 0;
    while(total_bytes_received < filesize){
        int bytes_read = read(nsd, buffer, sizeof(buffer)); // Read a chunk of the file from the client
        if(bytes_read < 0){
            printf("Dirty Read\n");
            break;
        }
        write(fd, buffer, bytes_read); // Write the chunk to the file
        total_bytes_received += bytes_read; // Update the total bytes received
    }
    
    sleep(4); // Simulate some processing time

    lock.l_type = F_UNLCK; // Unlock the file
    fcntl(fd, F_SETLK, &lock); // Release the lock
    close(fd);

    // update metadata
    struct Meta meta;
    strcpy(meta.filename, filename);
    strcpy(meta.author, thread_args->session.username);
    meta.is_deleted=0;
    int mfd2=open(METADATA_FILES, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(mfd2 < 0){
        printf("Database issue\n");
        free(thread_args);
        return NULL;
    }
    write(mfd2, &meta, sizeof(struct Meta)); // Write the new file metadata to the file
    close(mfd2);
    printf("Upload command handled for %s\n", thread_args->session.username);
    free(thread_args);
    return NULL;
}



// download function
// this uses semaphore to limit the bandwidth to at max 3 downloads at a time
// client sends filename[200] 
// server sends filesize and then the file in chunks of 1000 bytes

void *handle_download(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;
    int nsd= thread_args->nsd;

    sem_wait(&download_sem); // Wait on the semaphore to limit concurrent downloads
    
    char filename[200];
    read(nsd, filename, sizeof(filename)); // Read the filename from the client

    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s/%s", FILES_DIR, filename); // Construct the file path

    int fd=open(filepath, O_RDONLY);
    if(fd<0){
        printf("File not found\n");
        int res=1;
        write(nsd, &res, sizeof(int));
        sem_post(&download_sem); // Release the semaphore
        free(thread_args);
        return NULL;
    }

    int res=0;
    write(nsd, &res, sizeof(int)); // Send success response to the client
    
    sleep(1); // Simulate some processing time
    int filesize = lseek(fd, 0, SEEK_END); // Get the filesize
    lseek(fd, 0, SEEK_SET); // Reset the file pointer to the beginning
    write(nsd, &filesize, sizeof(int)); // Send the filesize to the client

    char buffer[BUFFER_SIZE];
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

    sleep(4); // Simulate some processing time

    close(fd);
    sem_post(&download_sem); // Release the semaphore

    printf("Download command handled for %s\n", thread_args->session.username);
    free(thread_args);
    return NULL;
}



// update function
// client sends filename[200] and new content in chunks of 1000 bytes
// server checks if the file exists and is not deleted, then updates the file with new content
void *handle_update(void* arg){
    // Implement the logic to update files or resources
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;
    int nsd= thread_args->nsd;

    char filename[200];
    read(nsd, filename, sizeof(filename)); // Read the filename from the client

    int mfd=open(METADATA_FILES, O_RDONLY);
    if(mfd<0){
        printf("Database issue\n");
        int res=1;
        write(nsd, &res, sizeof(int));
        free(thread_args);
        return NULL;
    }

    struct Meta meta;
    int found=0;
    while(read(mfd, &meta, sizeof(struct Meta)) > 0){
        if(strcmp(meta.filename, filename) == 0 && meta.is_deleted == 0){
            found=1;
            break;
        }
    }
    close(mfd);

    if(found==0){
        printf("File not found\n");
        int res=1;
        write(nsd, &res, sizeof(int));
        free(thread_args);
        return NULL;
    }
    if(strcmp(meta.author, thread_args->session.username) != 0){
        printf("Unauthorized update attempt\n");
        int res=2;
        write(nsd, &res, sizeof(int));
        free(thread_args);
        return NULL;
    }

    int res=0;
    write(nsd, &res, sizeof(int)); // Send success response to the client

    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s/%s", FILES_DIR, filename); // Construct the file path

    int fd=open(filepath, O_WRONLY | O_TRUNC);
    if(fd<0){
        printf("File not found\n");
        int res=1;
        write(nsd, &res, sizeof(int));
        free(thread_args);
        return NULL;
    }

    struct flock lock;
    lock.l_type = F_WRLCK; // Write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    fcntl(fd, F_SETLKW, &lock); // Acquire the lock - blocks if another update is in progress

    sleep(1); // Simulate some processing time

    int filesize;   
    read(nsd, &filesize, sizeof(int)); // Read the filesize from the client

    char buffer[BUFFER_SIZE];
    int total_bytes_received = 0;
    while(total_bytes_received < filesize){
        int bytes_read = read(nsd, buffer, sizeof(buffer)); // Read a chunk of the file from the client
        if(bytes_read < 0){
            printf("Dirty Read\n");
            break;
        }
        write(fd, buffer, bytes_read); // Write the chunk to the file
        total_bytes_received += bytes_read; // Update the total bytes received
    }

    sleep(4); // Simulate some processing time  

    lock.l_type = F_UNLCK; // Unlock the file
    fcntl(fd, F_SETLK, &lock); // Release the lock
    close(fd);


    printf("Update command handled for %s\n", thread_args->session.username);
    free(thread_args);
    return NULL;
}



// delete function
// client sends filename[200]
// server sends 0-success and 1-not found 2-not admin
void* handle_delete(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;
    int nsd= thread_args->nsd;

    char filename[200];
    read(nsd, filename, sizeof(filename)); // Read the filename from the client

    if(strcmp(thread_args->session.role, "admin") != 0){
        printf("Unauthorized delete attempt\n");
        int res=2;
        write(nsd, &res, sizeof(int));
        free(thread_args);
        return NULL;
    }

    int mfd=open(METADATA_FILES, O_RDWR);
    if(mfd<0){
        printf("Database issue\n");
        int res=1;
        write(nsd, &res, sizeof(int));
        free(thread_args);
        return NULL;
    }

    struct Meta meta;
    int found=0;

    while(read(mfd, &meta, sizeof(struct Meta)) > 0){
        if(strcmp(meta.filename, filename) == 0 && meta.is_deleted == 0){
            found=1;
            meta.is_deleted=1;
            lseek(mfd, -sizeof(struct Meta), SEEK_CUR); // Move the file pointer back to the beginning of the record
            write(mfd, &meta, sizeof(struct Meta)); // Update the record with is_deleted=1
            break;
        }
    }
    close(mfd);

    if(!found){
        printf("File not found\n");
        int res=1;
        write(nsd, &res, sizeof(int));
        free(thread_args);
        return NULL;
    }

    int res=0;
    write(nsd, &res, sizeof(int)); // Send success response to the client

    printf("Delete command handled for %s\n", thread_args->session.username);
    free(thread_args);
    return NULL;
}