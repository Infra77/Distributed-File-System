#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include "client_fn.h"

// default server IP and port
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int main(){
    int sd;
    struct sockaddr_in serv;

    serv.sin_family = AF_INET;
    serv.sin_port = htons(SERVER_PORT);
    serv.sin_addr.s_addr = inet_addr(SERVER_IP);

    sd=socket(AF_INET,SOCK_STREAM,0);
    if(sd<0){
        printf("Error creating socket\n");
        return 1;
    }

    if(connect(sd,(struct sockaddr *)&serv,sizeof(serv))<0){
        printf("Error connecting to server\n");
        return 1;
    }

    struct Session session;
    memset(&session, 0, sizeof(session)); 

    printf("=== Distributed File System ===\n");
    printf("1. Login\n");
    printf("2. Signup\n");
    printf("3. Exit\n");
    printf("Choice: ");
    int choice;
    if(scanf("%d", &choice) != 1) return 1;

    if(choice == 3){
        printf("Exiting...\n");
        close(sd);
        return 0;
    }

    // Get user credentials and role
    printf("Username: "); scanf("%49s", session.username);
    printf("Password: "); scanf("%49s", session.password);
    printf("Role (admin/user): "); scanf("%9s", session.role);

    if(choice == 2 && strcmp(session.role, "admin") == 0) {
        printf("Signup failed: You cannot register as an admin.\n");
        close(sd);
        return 1;
    }

    int res = authenticate(sd, &session, choice);
    if(res != 0){
        printf("Authentication failed.\n");
        close(sd);
        return 1;
    }

    printf("Welcome, %s!\n", session.username);

    char userdir[200];
    snprintf(userdir, sizeof(userdir), "./%s", session.username);
    mkdir(userdir, 0777);

    char cmd[200];
    int c; 
    while ((c = getchar()) != '\n' && c != EOF); 
    
    // Main command loop after successful authentication
    while(1){
        printf("%s/dfs> ", session.username);
        
        memset(cmd, 0, sizeof(cmd));        // Clear the command buffer
        if(fgets(cmd, sizeof(cmd), stdin) == NULL) break;
        cmd[strcspn(cmd, "\n")] = 0;

        if (strlen(cmd) == 0) continue;

        struct Thread_Args arg;
        memset(&arg, 0, sizeof(arg)); 

        // Set up thread arguments
        arg.sd = sd;
        arg.session = session;

        // Create a thread for each command to allow concurrent execution
        pthread_t tid;

        // Remote Server Commands
        if(strcmp(cmd, "list")==0){
            pthread_create(&tid, NULL, list, (void*)&arg);
            pthread_join(tid, NULL);
        }
        else if(strncmp(cmd, "upload ", 7)==0){
            strncpy(arg.filename, cmd + 7, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, upload, (void*)&arg);
            pthread_join(tid, NULL);
        }
        else if(strncmp(cmd, "download ", 9)==0){
            strncpy(arg.filename, cmd + 9, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, download, (void*)&arg);
            pthread_join(tid, NULL);
        }
        else if(strncmp(cmd, "update ", 7)==0){
            strncpy(arg.filename, cmd + 7, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, update, (void*)&arg);
            pthread_join(tid, NULL);
        }
        else if(strncmp(cmd, "delete ", 7)==0){
            strncpy(arg.filename, cmd + 7, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, delete, (void*)&arg);
            pthread_join(tid, NULL);
        }

        // Local Sandbox Commands
        else if(strcmp(cmd, "ls")==0){
            pthread_create(&tid, NULL, local_ls, (void*)&arg);
            pthread_join(tid, NULL);
        }
        else if(strncmp(cmd, "touch ", 6)==0){
            strncpy(arg.filename, cmd + 6, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, local_touch, (void*)&arg);
            pthread_join(tid, NULL);
        }
        else if(strncmp(cmd, "cat ", 4)==0){
            strncpy(arg.filename, cmd + 4, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, local_cat, (void*)&arg);
            pthread_join(tid, NULL);
        }
        else if(strcmp(cmd, "exit")==0){
            write(sd, cmd, 200); 
            printf("Exiting...\n");
            break;
        }

        // Help command to list available commands
        else if(strcmp(cmd, "help") == 0){
            printf("  --- Server Commands ---\n");
            printf("  list, upload <file>, download <file>, update <file>, delete <file>\n");
            printf("  --- Local Commands ---\n");
            printf("  ls, touch <file>, cat <file>\n");
            printf("  exit\n");
        }
        else{
            printf("Unknown command. Type 'help'.\n");
        }
    }

    close(sd);
    return 0;
}