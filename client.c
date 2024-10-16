/*
** client.c
*/

#include <stdio.h> // printf, perror
#include <stdlib.h> // exit (et atoi, malloc)
#include <unistd.h> // open et close
#include <string.h> // strlen
#include <sys/socket.h> // constantes AF_, PF_, SOCK_, toutes les fonctions (accept, listen, connect, socket)
#include <sys/types.h>
#include <netinet/in.h> // struct sockaddr_in[6]
#include <netdb.h> // addrinfo
#include <arpa/inet.h> // htons
#include <pthread.h>

#include "queue.h"

#define PORT "3114"

struct ThreadArgsRecv {
    int sockfd;
};

struct ThreadArgsSend {
    int sockfd;
};

void* handle_recv(void* arg) {
    struct ThreadArgsRecv* args = (struct ThreadArgsRecv*) arg;
    int sockfd = args->sockfd;

    char name[MAX_SIZE_NAME];
    memset(name, '\0', MAX_SIZE_NAME);
    char message[MAX_SIZE_MESSAGE];
    memset(message, '\0', MAX_SIZE_MESSAGE);

    char buf[MAX_SIZE_MESSAGE+MAX_SIZE_NAME];
    memset(buf, '\0', MAX_SIZE_MESSAGE+MAX_SIZE_NAME);

    int error = 0;
    socklen_t len = sizeof (error);

    while (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 &&
            error == 0 && 
            strcmp(message, "exit") != 0) {
        recv(sockfd, buf, MAX_SIZE_MESSAGE+MAX_SIZE_NAME, 0);
        strncpy(name, buf, MAX_SIZE_NAME);
        strncpy(message, buf+MAX_SIZE_NAME, MAX_SIZE_MESSAGE);
        printf("%s : %s\n", name, message);

        memset(buf, '\0', MAX_SIZE_MESSAGE+MAX_SIZE_NAME);
    }

    return NULL;
}

void* handle_send(void* arg) {
    return NULL;
}

int main(int argc, char* argv[]) {
    int sockfd;
    struct addrinfo hints, *server_addr, *p;
    int rv;
    char server_inet[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr, "Usage : client hostname\n");
        exit(1);
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(argv[1], PORT, &hints, &server_addr)) != 0) {
        fprintf(stderr, "getaddrinfo : %s\n", gai_strerror(rv));
        exit(1);
    }
    printf("adresse du serveur récupérée\n");

    for (p = server_addr; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("erreur socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("erreur connect");
            continue;
        }

        break;
    }
    if (p == NULL) {
        fprintf(stderr, "erreur connection\n");
        exit(1);
    }

    void* serv_ip;
    if (p->ai_addr->sa_family == AF_INET) {
        serv_ip = &((struct sockaddr_in*)(p->ai_addr))->sin_addr;
    }
    else {
        serv_ip = &((struct sockaddr_in6*)(p->ai_addr))->sin6_addr;
    }
    inet_ntop(p->ai_family, serv_ip, server_inet, sizeof(server_inet));
    printf("client connecting to %s\n", server_inet);

    freeaddrinfo(server_addr);

    pthread_t thread_send, thread_recv;

    struct ThreadArgsSend* args_send = (struct ThreadArgsSend*)malloc(sizeof(struct ThreadArgsSend));
    args_send->sockfd = sockfd;
    pthread_create(&thread_send, NULL, handle_send, args_send);

    struct ThreadArgsRecv* args_recv = (struct ThreadArgsRecv*)malloc(sizeof(struct ThreadArgsRecv));
    args_recv->sockfd = sockfd;
    pthread_create(&thread_recv, NULL, handle_recv, args_recv);

    send(sockfd, "Simon\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0salut", 26, 0);

    pthread_join(thread_recv, NULL);
    pthread_cancel(thread_send);
    pthread_join(thread_send, NULL);

    close(sockfd);

    return 0;
}