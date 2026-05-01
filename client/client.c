#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include "client_fn.h"

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

    printf("=== Distributed File System ===\n");

    printf("1. Login\n");
    printf("2. Signup\n");
    printf("3. Exit\n");
    printf("Choice: ");
    int choice;
    scanf("%d", &choice);

    if(choice == 3){
        printf("Exiting...\n");
        return 0;
    }

    printf("Username: "); scanf("%49s", session.username);
    printf("Password: "); scanf("%49s", session.password);
    printf("Role (admin/user): "); scanf("%9s", session.role);


    int res=authenticate(sd, &session, choice);
    if(res!=0){
        printf("Authentication failed\n");
        close(sd);
        return 1;
    }

    printf("Welcone, %s!\n", session.username);

    char userdir[200];
    sprintf(userdir, "./client/%s", session.username);
    mkdir(userdir, 0755);

    // CLI loop - same sd used for all commands
    char cmd[200];
    
    getchar();

    while(1){
        printf("%s/dfs> ", session.username);
        
        if(fgets(cmd, sizeof(cmd), stdin) == NULL) break;
        cmd[strcspn(cmd, "\n")] = 0;  // strip trailing newline

        struct Thread_Args arg;
        arg.sd = sd;
        arg.session = session;

        pthread_t tid;

        if(strcmp(cmd, "list")==0){
            arg.filename[0] = '\0';
            pthread_create(&tid, NULL, list, (void*)&arg);
            pthread_join(tid, NULL);

        }
        else if(strncmp(cmd, "upload ", 7)==0){
            char *filename = cmd + 7;
            strncpy(arg.filename, filename, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, upload, (void*)&arg);
            pthread_join(tid, NULL);
        }
        else if(strncmp(cmd, "download ", 9)==0){
            char *filename = cmd + 9;
            strncpy(arg.filename, filename, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, download, (void*)&arg);
            pthread_join(tid, NULL);

        }
        else if(strncmp(cmd, "update ", 7)==0){
            char *filename = cmd + 7;
            strncpy(arg.filename, filename, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, update, (void*)&arg);
            pthread_join(tid, NULL);

        }
        else if(strncmp(cmd, "delete ", 7)==0){
            char *filename = cmd + 7;
            strncpy(arg.filename, filename, sizeof(arg.filename)-1);
            pthread_create(&tid, NULL, delete, (void*)&arg);
            pthread_join(tid, NULL);

        }
        else if(strcmp(cmd, "exit")==0){
            write(sd, cmd, strlen(cmd));
            printf("Exiting...\n");
            break;
        }
        else if(strcmp(cmd, "help") == 0){
            printf("  list\n");
            printf("  upload   <filename>\n");
            printf("  download <filename>\n");
            printf("  update   <filename>\n");
            printf("  delete   <filename>\n");
            printf("  exit\n");
            continue;
        }
        else{
            printf("Unknown command. Type 'help' for usage.\n");
        }
    }

    close(sd);
    printf("Goodbye!\n");
    return 0;
}