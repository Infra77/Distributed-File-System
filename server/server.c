#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include "server_fn.h"

#define PORT 8080
#define BUFFER_SIZE 1000

sem_t download_sem;
pthread_mutex_t meta_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t user_mutex = PTHREAD_MUTEX_INITIALIZER;

void handle_client(int nsd, struct Session session){
    char cmd[200];
    
    while(1){
        int bytes_read = read(nsd, cmd, sizeof(cmd)); 
        if (bytes_read <= 0) {
            printf("Client %s disconnected.\n", session.username);
            break;
        }

        printf("Command received from %s: %s\n", session.username, cmd);
    
        struct Thread_Args *thread_args = (struct Thread_Args *)malloc(sizeof(struct Thread_Args));
        thread_args->nsd = nsd;
        thread_args->session = session;
    
        pthread_t tid;
    
        if (strcmp(cmd, "list") == 0) {
            pthread_create(&tid, NULL, handle_list, thread_args);
            pthread_join(tid, NULL);
        }
        else if (strcmp(cmd, "upload") == 0) {
            pthread_create(&tid, NULL, handle_upload, thread_args);
            pthread_join(tid, NULL);
        }
        else if (strcmp(cmd, "download") == 0) {
            pthread_create(&tid, NULL, handle_download, thread_args);
            pthread_join(tid, NULL);
        }
        else if (strcmp(cmd, "update") == 0) {
            pthread_create(&tid, NULL, handle_update, thread_args);
            pthread_join(tid, NULL);
        }
        else if (strcmp(cmd, "delete") == 0) {
            pthread_create(&tid, NULL, handle_delete, thread_args);
            pthread_join(tid, NULL);
        }
        else if (strcmp(cmd, "exit") == 0) {
            free(thread_args);
            break;
        }
        else {
            char response[BUFFER_SIZE];
            strcpy(response, "Invalid command");
            write(nsd, response, sizeof(response));
        }
        
        if (strcmp(cmd, "exit") != 0) {
            free(thread_args);
        }
    }
}

void *client_thread(void* arg){
    int nsd = *((int *)arg);
    free(arg);

    struct Session session;
    if(handle_auth(nsd, &session) == 0){
        handle_client(nsd, session);
    } else {
        printf("Authentication failed for a connection.\n");
    }

    close(nsd);
    return NULL;
}

int main(){
    // Setup file storage relative to current directory
    mkdir("files", 0777);

    sem_init(&download_sem, 0, 3); 

    int sd, nsd;
    struct sockaddr_in serv, cli;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT);
    serv.sin_addr.s_addr = INADDR_ANY;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if(sd < 0){
        perror("socket error"); exit(1);
    }

    int opt = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sd, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("bind failed"); exit(1);
    }

    listen(sd, 5);
    printf("Server is listening on port %d...\n", PORT);

    while(1){
        socklen_t clilen = sizeof(cli);
        nsd = accept(sd, (struct sockaddr *)&cli, &clilen);

        if(nsd < 0){
            perror("accept error"); continue;
        }

        printf("Connection accepted from %s\n", inet_ntoa(cli.sin_addr));

        int *nsd_ptr = malloc(sizeof(int));
        *nsd_ptr = nsd;
        
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, nsd_ptr);
        pthread_detach(tid); 
    }
        
    sem_destroy(&download_sem); 
    close(sd);
    return 0;
}