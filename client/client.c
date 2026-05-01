#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "client_fn.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int main(){
    struct Session session;

    printf("=== Distributed File System ===\n");

    printf("1. Login\n");
    printf("2. Signup\n");
    printf("3. Exit\n");
    printf("Choice: ");
    int choice;
    scanf("%d", &choice);

    
}