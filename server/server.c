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

void handle_client(int nsd, struct Session session){
    char cmd[200];
    
    while(1){
        read(nsd, cmd, sizeof(cmd)); 
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
        else{
            char response[BUFFER_SIZE];
            strcpy(response, "Invalid command");
            write(nsd, response, sizeof(response));
            free(thread_args);
        }
    }
}


void *client_thread(void* arg){
    int nsd= *((int *)arg);
    free(arg);

    struct Session session;
    if(handle_auth(nsd, &session)==0){
        handle_client(nsd, session);
    }

    close(nsd);
    return NULL;
}


int main(){

    mkdir("./server/files", 0777); // Create directory for storing files if it doesn't exist

    sem_init(&download_sem, 0, 3); // Initialize the semaphore for download synchronization

    int sd;
    int nsd;

    struct sockaddr_in serv, cli;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8080);
    serv.sin_addr.s_addr = INADDR_ANY;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if(sd < 0){
        perror("socket");
        exit(1);
    }

    bind(sd, (struct sockaddr *)&serv, sizeof(serv));

    listen(sd, 5);

    printf("Server is listening on port 8080...\n");
    while(1){
        struct sockaddr_in cli;
        int clilen=sizeof(cli);
        nsd = accept(sd, (struct sockaddr *)&cli, &clilen);

        if(nsd<0){
            perror("accept");
            continue;
        }

        printf("Connection accepted\n");

        int *nsd_ptr=malloc(sizeof(int));
        *nsd_ptr=nsd;
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, nsd_ptr);
        pthread_detach(tid); // Detach the thread to allow for automatic resource cleanup
    }
        
    sem_destroy(&download_sem); // Destroy the semaphore
    close(sd);
    return 0;
}