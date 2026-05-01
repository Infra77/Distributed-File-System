#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include  "client_fn.h"

int main(){
    int sd;
    struct sockaddr_in serv;

    serv.sin_family = AF_INET;
    serv.sin_port = htons(8080);

    return 0;
}