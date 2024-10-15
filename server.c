/*
** server.c
*/

#include <stdio.h> // printf, perror
// perror écrit message dans stderr et complète avec errno
#include <stdlib.h> // exit (et atoi, malloc)
#include <unistd.h> // open et close
#include <string.h> // strlen
#include <sys/socket.h> // constantes AF_, PF_, SOCK_, toutes les fonctions (accept, listen, connect, socket)
#include <sys/types.h>
#include <netinet/in.h> // struct sockaddr_in[6]
#include <netdb.h> // addrinfo
#include <arpa/inet.h> // htons
#include <ctype.h> // pour print_ascii()
#include <pthread.h>

#include "queue.h"

#define PORT "3114"
// doit être un char*
#define MAX_LOG 5

typedef struct ThreadArgs {
    int connfd;
    int thread_id;
    Queue* queue;
} ThreadArgs;

void print_ascii(const char *data, int len) {
    for (int i = 0; i < len; i++) {
        if (isprint(data[i])) {
            printf("%c", data[i]); // Print printable characters as is
        } else {
            printf("\\x%02X", (unsigned char)data[i]); // Print non-printable in \xHH format
        }
    }
    printf("\n");
}

void remove_trailing_non_printable(char* string, int len) {
    int i = len;
    while (i > 0) {
        if (string[i] != '\r' && string[i] != '\n' && string[i] != '\0') {
            return;
        }
        string[i] = '\0';
        i--;
    }
}

void* handle_client(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int connfd = args->connfd;
    int thread_id = args->thread_id;
    Queue* queue = args->queue;

    char name[MAX_SIZE_NAME];
    memset(name, '\0', MAX_SIZE_NAME);
    snprintf(name, MAX_SIZE_NAME, "client %d", thread_id);

    char buf[MAX_SIZE_MESSAGE];
    memset(buf, '\0', MAX_SIZE_MESSAGE);
    int bytes_recus;

    printf("thread %d : connexion faite\n", thread_id);

    while ((bytes_recus = recv(connfd, buf, MAX_SIZE_MESSAGE-1, 0)) > 0) {
        remove_trailing_non_printable(buf, bytes_recus);

        printf("thread %d : reception de ", thread_id);
        print_ascii(buf, strlen(buf));

        Message message;
        strcpy(message.name, name);
        strcpy(message.buf, buf);
        queueAdd(queue, message);
        
        memset(buf, '\0', MAX_SIZE_MESSAGE);
    }

    printf("thread %d : fin de la connexion\n", thread_id);
    close(connfd); // on ferme la connexion
    return NULL;
}

void* watch_queue(void* arg) {
    Queue* queue = (Queue*)arg;
    while (1) {
        if (!queueIsEmpty(queue)) {
            Message message = queueRemove(queue);
            printf("%s : ", message.name);
            print_ascii(message.buf, strlen(message.buf));
        }
    }
    return NULL;
}

int main() {
    int sockfd, connfd;
    // listen sur sockfd et accept les nouvelles connexions sur connfd
    struct addrinfo hints, *servinfo, *p;
    // hints pour avoir infos sur soi-même, qui vont aller dans ervinfo
    // p va servir à parcourir les servinfo proposés pour voir lequel on arrive à bind
    struct sockaddr_storage client_sa;
    // storage pour avoir la place pour les 2 types (ipv4 et 6)
    socklen_t sin_size;
    // taille de l'adresse du client
    int yes = 1;
    // pour la fonction qui vérifie si le port est libre
    void* client_in_address;
    char client_ip[INET6_ADDRSTRLEN];
    int rv;
    // retour de getaddrinfo pour afficher les erreurs
    char message[MAX_SIZE_MESSAGE];
    memset(message, '\0', MAX_SIZE_MESSAGE);
    char buf[MAX_SIZE_MESSAGE];
    memset(buf, '\0', MAX_SIZE_MESSAGE);
    int bytes_recus;

    Queue* queue = queueCreate(10);

    memset(&hints, 0, sizeof hints); // on nettoie hints
    hints.ai_family = AF_UNSPEC; // ipv4 ou 6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE; // on prend notre propre adresse ip
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo : %s\n", gai_strerror(rv));
        exit(1);
    }
    printf("adresse du serveur récupérée\n");

    // on parcourt la linked list jusqu'à réussir à bind l'adresse
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("erreur pendant socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            // on dit qu'on veut utiliser le port
            // marchera si on redémarre
            perror("port déjà utilisé");
            exit(1);
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("erreur pendant bind");
            exit(1);
        }
        // tout s'est bien passé
        // on peut arrêter de parcourir les adresses
        break;
    }
    freeaddrinfo(servinfo);
    if (p == NULL) {
        fprintf(stderr, "pas réussi à bind");
        exit(1);
    }
    printf("bind fait\n");

    if (listen(sockfd, MAX_LOG) == -1) {
        perror("erreur pendant listen");
        exit(1);
    }

    pthread_t watch_thread;
    pthread_create(&watch_thread, NULL, watch_queue, queue);

    printf("en attente de connexion...\n");
    while (1) {
        sin_size = sizeof client_sa;
        connfd = accept(sockfd, (struct sockaddr*)&client_sa, &sin_size);
        if (connfd == -1) {
            perror("erreur pendant accept");
            continue;
        }

        if (((struct sockaddr*)&client_sa)->sa_family == AF_INET) {
            // on a reçu une adresse ipv4
            client_in_address = &(((struct sockaddr_in*)&client_sa)->sin_addr);
        }
        else {
            // on a reçu une ipv6
            client_in_address = &(((struct sockaddr_in6*)&client_sa)->sin6_addr);
        }
        inet_ntop(client_sa.ss_family, client_in_address, client_ip, sizeof client_ip);
        printf("%s s'est connecté\n", client_ip);

        pthread_t thread;
        ThreadArgs* args = malloc(sizeof(ThreadArgs));
        args->connfd = connfd;
        args->thread_id = 0;
        args->queue = queue;
        pthread_create(&thread, NULL, handle_client, args);
    }

    pthread_cancel(watch_thread);
    pthread_join(watch_thread, NULL);

    queueDestroy(queue);

    return 0;
}