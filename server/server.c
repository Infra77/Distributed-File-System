#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server_fn.h"
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1000

pthread_t list_threads[100];
pthread_t download_threads[100];
pthread_t upload_threads[100];
pthread_t update_threads[100];
int list_thread_count = 0;
int download_thread_count = 0;  
int upload_thread_count = 0;
int update_thread_count = 0;

void handle_client(int nsd){
    char cmd[200];
    
    read(nsd, cmd, sizeof(cmd)); 
    printf("Command received: %s\n", cmd);

    if(strcmp(cmd, "list")==0){
        pthread_create(&list_threads[list_thread_count++], NULL, (void *)handle_list, (void *)nsd);
    }
    else if(strncmp(cmd, "download", 8)==0){
        pthread_create(&download_threads[download_thread_count++], NULL, (void *)handle_download, (void *)nsd);
    }
    if(strncmp(cmd, "upload", 6)==0){
        pthread_create(&upload_threads[upload_thread_count++], NULL, (void *)handle_upload, (void *)nsd);
    }
    else if(strncmp(cmd, "update", 6)==0){
        pthread_create(&update_threads[update_thread_count++], NULL, (void *)handle_update, (void *)nsd);
    }
    else{
        char response[BUFFER_SIZE];
        strcpy(response, "Invalid command");
        write(nsd, response, sizeof(response));
    }
}
int main(){
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
        socklen_t cli_len = sizeof(cli);
        nsd = accept(sd, (struct sockaddr *)&cli, &cli_len);
        if(nsd < 0){
            perror("accept");
            continue;
        }

        printf("Client connected\n");
        handle_auth(nsd);
        handle_client(nsd);

        close(nsd);
    }
        
}