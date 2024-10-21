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

#define PORT "3114"
#define MAX_SIZE_MESSAGE 100
#define MAX_SIZE_NAME 20


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[]) {
    int remotefd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char serverIP[INET6_ADDRSTRLEN];

    char buf_recv[MAX_SIZE_NAME+MAX_SIZE_MESSAGE];
    memset(buf_recv, '\0', MAX_SIZE_NAME+MAX_SIZE_MESSAGE);

    char buf_send[MAX_SIZE_NAME+MAX_SIZE_MESSAGE];
    memset(buf_send, '\0', MAX_SIZE_NAME+MAX_SIZE_MESSAGE);

    fd_set read_fds;

    if (argc != 2) {
        fprintf(stderr, "Usage : client <hostname>\n");
        exit(1);
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo : %s\n", gai_strerror(rv));
        exit(1);
    }
    printf("adresse du serveur récupérée\n");

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((remotefd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("erreur socket");
            continue;
        }

        if (connect(remotefd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("erreur connect");
            continue;
        }

        break;
    }
    if (p == NULL) {
        fprintf(stderr, "erreur connection\n");
        exit(1);
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)&p), serverIP, sizeof(serverIP));
    printf("Connexion à %s\n", serverIP);

    freeaddrinfo(servinfo);

    printf("Entrez votre nom : ");
    fgets(buf_send, MAX_SIZE_NAME, stdin);
    buf_send[strlen(buf_send)-1] = '\0';

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(remotefd, &read_fds);

        if (select(remotefd+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("erreur select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            fgets(buf_send+MAX_SIZE_NAME, MAX_SIZE_MESSAGE, stdin);
            buf_send[MAX_SIZE_NAME+strlen(buf_send+MAX_SIZE_NAME)-1] = '\0';

            send(remotefd, buf_send, MAX_SIZE_NAME+MAX_SIZE_MESSAGE, 0);

            memset(buf_send+MAX_SIZE_NAME, '\0', MAX_SIZE_MESSAGE);
        }

        if (FD_ISSET(remotefd, &read_fds)) {
            if (recv(remotefd, buf_recv, MAX_SIZE_MESSAGE+MAX_SIZE_NAME, 0) <= 0) {
                printf("connexion fermée par le serveur\n");
                break;
            }
            if (strncmp(buf_recv+MAX_SIZE_NAME, "exit", MAX_SIZE_MESSAGE) == 0) {
                printf("déconnexion\n");
                break;
            }
            printf("%s : %s\n", buf_recv, buf_recv+MAX_SIZE_NAME);

            memset(buf_recv, '\0', MAX_SIZE_MESSAGE+MAX_SIZE_NAME);
        }
    }

    close(remotefd);

    return 0;
}