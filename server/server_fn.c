#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include "server_fn.h"


// default Server configurations
#define PORT 8080
#define BUFFER_SIZE 1000
#define USER_FILES "users.txt"
#define METADATA_FILES "metadata.txt"
#define FILES_DIR "files"

// Global concurrency controls
extern sem_t download_sem;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;


// Function to log user actions to a file with timestamps, ensuring thread safety with a mutex lock.
void log_action(const char* username, const char* action, const char* filename) {
    pthread_mutex_lock(&log_mutex);
    FILE *fp = fopen("logs.txt", "a");
    if (fp) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        fprintf(fp, "[%02d:%02d:%02d] User: %-15s | Action: %-10s | File: %s\n", 
                t->tm_hour, t->tm_min, t->tm_sec, username, action, filename ? filename : "N/A");
        fclose(fp);
    }
    pthread_mutex_unlock(&log_mutex);
}


// Function to handle user authentication (login/signup) based on the choice sent by the client
int handle_auth(int nsd, struct Session *session){
    // Read the authentication choice (1 for login, 2 for signup) and user credentials from the client
    int choice;
    read(nsd, &choice, sizeof(int)); 

    // Read username, password, and role from the client to create a Session struct for authentication. This information will be used to verify credentials for login or to create a new user for signup.
    char username[50], password[50], role[10];
    read(nsd, username, sizeof(username)); 
    read(nsd, password, sizeof(password)); 
    read(nsd, role, sizeof(role)); 

    struct Session auth;
    memset(&auth, 0, sizeof(auth)); 
    strcpy(auth.username, username);
    strcpy(auth.password, password);
    strcpy(auth.role, role);

    // Lock the user_mutex to ensure thread-safe access to the user data file during authentication processes (both login and signup).
    pthread_mutex_lock(&user_mutex);

    if(choice == 1){ 
        // login
        int fd = open(USER_FILES, O_RDONLY);
        if(fd < 0){
            int res=1; write(nsd, &res, sizeof(int));
            pthread_mutex_unlock(&user_mutex);
            return 1;
        }

        // Read through the user data file to find a matching username and password for login authentication. If a match is found, populate the session struct with the user's details and return success; otherwise, return failure.
        struct Session rec;
        while(read(fd, &rec, sizeof(struct Session)) > 0){
            if(strcmp(auth.username, rec.username) == 0 && strcmp(auth.password, rec.password) == 0){
                int res = 0; write(nsd, &res, sizeof(int));
                close(fd);
                
                strcpy(session->username, rec.username);
                strcpy(session->password, rec.password);
                strcpy(session->role, rec.role); 
                
                pthread_mutex_unlock(&user_mutex);
                log_action(session->username, "LOGIN", NULL);
                return 0;
            }
        }
        int res=1; write(nsd, &res, sizeof(int));
        close(fd);
        pthread_mutex_unlock(&user_mutex);
        return 1;
    }

    if(choice == 2){ 
        // signup
        int fd = open(USER_FILES, O_RDONLY);
        if (fd >= 0) {
            // Check if the username already exists in the user data file during signup. If it does, return failure; otherwise, append the new user information to the file and return success.
            struct Session rec;
            while (read(fd, &rec, sizeof(struct Session)) > 0) {
                if (strcmp(rec.username, auth.username) == 0) {
                    close(fd);
                    int res=1; 
                    write(nsd, &res, sizeof(int));
                    pthread_mutex_unlock(&user_mutex);
                    return 1;
                }
            }
            close(fd);
        }
        
        // Append the new user's session information to the user data file for signup. This includes the username, password, and role, which will be used for future authentication.
        fd = open(USER_FILES, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(fd < 0){
            int res=1; write(nsd, &res, sizeof(int));
            pthread_mutex_unlock(&user_mutex);
            return 1;
        }
        write(fd, &auth, sizeof(struct Session)); 
        close(fd);
        int res=0; 
        write(nsd, &res, sizeof(int));

        strcpy(session->username, auth.username);
        strcpy(session->password, auth.password);
        strcpy(session->role, auth.role);

        pthread_mutex_unlock(&user_mutex);
        log_action(session->username, "SIGNUP", NULL);
        return 0;
    }
    
    pthread_mutex_unlock(&user_mutex);
    return 1;
}


// Function to list files available on the server, sending the file metadata (filename, author, deletion status) to the client.
void *handle_list(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;
    int nsd = thread_args->nsd;
    
    pthread_mutex_lock(&meta_mutex); 
    int fd = open(METADATA_FILES, O_RDONLY);
    if(fd < 0){
        int count=0; write(nsd, &count, sizeof(int));
        pthread_mutex_unlock(&meta_mutex);
        return NULL; 
    }

    // Count the number of non-deleted files in the metadata file and send this count to the client. This allows the client to know how many files are available for listing.
    struct Meta rec;
    int count=0;
    while(read(fd, &rec, sizeof(struct Meta)) > 0){
        if(rec.is_deleted == 0) count++;
    }
    
    write(nsd, &count, sizeof(int)); 
    lseek(fd, 0, SEEK_SET); 

    while(read(fd, &rec, sizeof(struct Meta)) > 0){
        if(rec.is_deleted == 0){
            // Send the filename, author, and deletion status of each non-deleted file to the client. This information is used by the client to display the list of available files.
            write(nsd, rec.filename, sizeof(rec.filename));
            write(nsd, rec.author, sizeof(rec.author));
            write(nsd, &rec.is_deleted, sizeof(int)); 
        }
    }
    close(fd);
    pthread_mutex_unlock(&meta_mutex);

    // Log the LIST action for the authenticated user, indicating that they requested a list of available files on the server.
    log_action(thread_args->session.username, "LIST", NULL);
    return NULL;
}


// Function to handle file upload from the client, ensuring that the file does not already exist and managing file metadata accordingly.
void *handle_upload(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;
    int nsd = thread_args->nsd;
    
    // Use a semaphore to limit the number of concurrent uploads to 3. This ensures that the server can manage resources effectively while allowing multiple clients to upload files simultaneously.
    sem_wait(&upload_sem);

    // Read the filename from the client for the file to be uploaded. This filename will be checked against existing files to prevent duplicates and will be used to create the new file on the server.
    char filename[200];
    read(nsd, filename, sizeof(filename)); 

    // While we upload a file, we use a mutex lock to ensure that the metadata file is accessed in a thread-safe manner
    pthread_mutex_lock(&meta_mutex);
    // Check if a file with the same name already exists in the metadata file and is not marked as deleted. If such a file exists, return an error to the client; otherwise, proceed with the upload process.
    int mfd = open(METADATA_FILES, O_RDONLY);
    if(mfd >= 0){
        struct Meta rec;
        while(read(mfd, &rec, sizeof(struct Meta)) > 0){
            if(strcmp(rec.filename, filename) == 0 && rec.is_deleted == 0){
                int res=1; 
                write(nsd, &res, sizeof(int));
                close(mfd);
                pthread_mutex_unlock(&meta_mutex);
                return NULL;
            }
        }
        close(mfd);
    }
    // unlock the meta_mutex after checking for existing files in the metadata, allowing other threads to access the metadata file while we proceed with the upload process.
    pthread_mutex_unlock(&meta_mutex);

    // Create the file on the server for the uploaded content, using file locking to ensure that the file is not being accessed by other threads during the upload process. This prevents data corruption and ensures that the file is created successfully.
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s/%s", FILES_DIR, filename); 

    int fd = open(filepath, O_WRONLY | O_CREAT | O_EXCL, 0666); 
    if(fd < 0){
        int res=1; 
        write(nsd, &res, sizeof(int));
        return NULL;
    }

    // Use file locking to ensure that the file is not being accessed by other threads during the upload process. This prevents data corruption and ensures that the file is created successfully.
    struct flock lock;
    lock.l_type = F_WRLCK; 
    lock.l_whence = SEEK_SET;
    lock.l_start = 0; lock.l_len = 0; 
    fcntl(fd, F_SETLKW, &lock); 

    int res=0; 
    write(nsd, &res, sizeof(int)); 

    // send the file size from the client and then read the file content in chunks, writing it to the newly created file on the server. This process continues until the entire file is received, ensuring that the uploaded file is stored correctly on the server.
    int filesize;
    read(nsd, &filesize, sizeof(int)); 

    char buffer[BUFFER_SIZE];
    // TCP Protocol - stream of data
    int total_bytes_received = 0;
    while(total_bytes_received < filesize){
        int bytes_read = read(nsd, buffer, sizeof(buffer)); 
        if(bytes_read <= 0) break;
        write(fd, buffer, bytes_read); 
        total_bytes_received += bytes_read; 
    }
    
    // After the file upload is complete, unlock the file and close the file descriptor to allow other threads to access the file if needed.
    lock.l_type = F_UNLCK; 
    fcntl(fd, F_SETLK, &lock); 
    close(fd);


    struct Meta meta;
    memset(&meta, 0, sizeof(meta));
    strcpy(meta.filename, filename);
    strcpy(meta.author, thread_args->session.username);
    meta.is_deleted = 0;
    
    // Lock the meta_mutex to ensure thread-safe access to the metadata file while appending the new file's metadata
    pthread_mutex_lock(&meta_mutex);
    int mfd2 = open(METADATA_FILES, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(mfd2 >= 0) {
        write(mfd2, &meta, sizeof(struct Meta)); 
        close(mfd2);
    }
    pthread_mutex_unlock(&meta_mutex);
    
    log_action(thread_args->session.username, "UPLOAD", filename);

    // Release the semaphore after the upload is complete, allowing other threads to proceed with their upload operations. This ensures that the server can manage concurrent uploads effectively while maintaining resource limits.
    sem_post(&upload_sem);
    return NULL;
}


// Function to handle file download requests from the client, ensuring that the requested file exists and managing file locks during the download process.
void *handle_download(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;
    int nsd = thread_args->nsd;

    // similar logic as upload.
    sem_wait(&download_sem);    
    
    char filename[200];
    read(nsd, filename, sizeof(filename)); 

    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s/%s", FILES_DIR, filename); 

    int fd = open(filepath, O_RDONLY);
    if(fd < 0){
        int res=1; write(nsd, &res, sizeof(int));
        sem_post(&download_sem); 
        return NULL;
    }

    struct flock lock;
    lock.l_type = F_RDLCK; 
    lock.l_whence = SEEK_SET;
    lock.l_start = 0; lock.l_len = 0; 
    fcntl(fd, F_SETLKW, &lock);

    int res = 0; write(nsd, &res, sizeof(int)); 
    
    int filesize = lseek(fd, 0, SEEK_END); 
    lseek(fd, 0, SEEK_SET); 
    write(nsd, &filesize, sizeof(int)); 

    char buffer[BUFFER_SIZE];
    int total_bytes_sent = 0;
    while(total_bytes_sent < filesize){
        int bytes_read = read(fd, buffer, sizeof(buffer)); 
        if(bytes_read <= 0) break;
        write(nsd, buffer, bytes_read); 
        total_bytes_sent += bytes_read; 
    }

    lock.l_type = F_UNLCK; 
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    
    log_action(thread_args->session.username, "DOWNLOAD", filename);
    
    sem_post(&download_sem); 
    return NULL;
}


// Function to handle file update requests from the client, ensuring that the requested file exists, the user has permission to update it, and managing file locks during the update process.
void *handle_update(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;
    int nsd = thread_args->nsd;

    char filename[200];
    read(nsd, filename, sizeof(filename)); 

    pthread_mutex_lock(&meta_mutex);
    int mfd = open(METADATA_FILES, O_RDONLY);
    struct Meta meta;
    int found=0;
    if(mfd >= 0){
        while(read(mfd, &meta, sizeof(struct Meta)) > 0){
            if(strcmp(meta.filename, filename) == 0 && meta.is_deleted == 0){
                found=1; break;
            }
        }
        close(mfd);
    }
    pthread_mutex_unlock(&meta_mutex);

    if(found == 0){
        int res=1; write(nsd, &res, sizeof(int));
        return NULL;
    }
    
    if(strcmp(meta.author, thread_args->session.username) != 0 && strcmp(thread_args->session.role, "admin") != 0){
        int res=2; write(nsd, &res, sizeof(int));
        return NULL;
    }

    int res=0; write(nsd, &res, sizeof(int)); 

    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s/%s", FILES_DIR, filename); 

    int fd = open(filepath, O_WRONLY | O_TRUNC);
    if(fd < 0){ return NULL; }

    struct flock lock;
    lock.l_type = F_WRLCK; 
    lock.l_whence = SEEK_SET;
    lock.l_start = 0; lock.l_len = 0; 
    fcntl(fd, F_SETLKW, &lock); 

    int filesize;   
    read(nsd, &filesize, sizeof(int)); 

    char buffer[BUFFER_SIZE];
    int total_bytes_received = 0;
    while(total_bytes_received < filesize){
        int bytes_read = read(nsd, buffer, sizeof(buffer)); 
        if(bytes_read <= 0) break;
        write(fd, buffer, bytes_read); 
        total_bytes_received += bytes_read; 
    }

    lock.l_type = F_UNLCK; 
    fcntl(fd, F_SETLK, &lock); 
    close(fd);

    log_action(thread_args->session.username, "UPDATE", filename);
    return NULL;
}


// Function to handle file deletion requests from the client, ensuring that the requested file exists, the user has permission to delete it, and managing file metadata accordingly.
void* handle_delete(void* arg){
    struct Thread_Args *thread_args = (struct Thread_Args *)arg;
    int nsd = thread_args->nsd;

    char filename[200];
    read(nsd, filename, sizeof(filename)); 

    pthread_mutex_lock(&meta_mutex);
    int mfd = open(METADATA_FILES, O_RDWR);
    if(mfd < 0){
        int res=1; write(nsd, &res, sizeof(int));
        pthread_mutex_unlock(&meta_mutex);
        return NULL;
    }

    struct Meta meta;
    int found=0;

    while(read(mfd, &meta, sizeof(struct Meta)) > 0){
        if(strcmp(meta.filename, filename) == 0 && meta.is_deleted == 0){
            found = 1;
            
            if(strcmp(meta.author, thread_args->session.username) != 0 && strcmp(thread_args->session.role, "admin") != 0){
                int res=2; 
                write(nsd, &res, sizeof(int));
                close(mfd);
                pthread_mutex_unlock(&meta_mutex);
                return NULL;
            }

            meta.is_deleted=1;
            lseek(mfd, -(off_t)sizeof(struct Meta), SEEK_CUR); 
            write(mfd, &meta, sizeof(struct Meta)); 
            break;
        }
    }
    close(mfd);
    pthread_mutex_unlock(&meta_mutex);

    if(!found){
        int res=1; write(nsd, &res, sizeof(int));
        return NULL;
    }

    int res=0; write(nsd, &res, sizeof(int)); 

    log_action(thread_args->session.username, "DELETE", filename);
    return NULL;
}