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

#define PORT "3114"
// doit être un char*
#define MAX_LOG 5
#define MAX_NUMBER_CLIENTS 10
#define MAX_SIZE_MESSAGE 100
#define MAX_SIZE_NAME 20

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

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main() {
    int listener, newfd;
    // listen sur listener et accept les nouvelles connexions sur newfd

    struct addrinfo hints, *servinfo, *p;
    // hints pour avoir infos sur soi-même, qui vont aller dans ervinfo
    // p va servir à parcourir les servinfo proposés pour voir lequel on arrive à bind

    struct sockaddr_storage remoteaddr; // storage pour avoir la place pour les 2 types (ipv4 et 6)
    socklen_t addrlen; // taille de l'adresse du client
    int yes = 1; // pour la fonction qui vérifie si le port est libre

    char remoteIP[INET6_ADDRSTRLEN];
    int rv; // retour de getaddrinfo pour afficher les erreurs

    char buf[MAX_SIZE_NAME+MAX_SIZE_MESSAGE];
    memset(buf, '\0', MAX_SIZE_NAME+MAX_SIZE_MESSAGE);

    fd_set all_fds, read_fds;
    int fdmax = 0;
    unsigned int number_clients = 0;
    FD_ZERO(&all_fds); // clear les sets
    FD_ZERO(&read_fds);

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
        if ((listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("erreur pendant socket");
            continue;
        }
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            // on dit qu'on veut utiliser le port
            // marchera si on redémarre
            close(listener);
            perror("port déjà utilisé");
            exit(1);
        }
        if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) {
            close(listener);
            perror("erreur pendant bind");
            exit(1);
        }
        // tout s'est bien passé
        // on peut arrêter de parcourir les adresses
        break;
    }
    freeaddrinfo(servinfo);
    if (p == NULL) {
        fprintf(stderr, "pas réussi à bind\n");
        close(listener);
        exit(1);
    }
    printf("bind fait\n");

    if (listen(listener, MAX_LOG) == -1) {
        perror("erreur pendant listen");
        close(listener);
        exit(1);
    }

    FD_SET(listener, &all_fds); // ajoute listener
    fdmax = listener;

    printf("en attente de connexion...\n");
    while (1) {
        read_fds = all_fds;

        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("erreur select");
            break;
        }

        for (int i = 0; i <= fdmax; ++i) {
            // on parcourt tous les fd pour voir lequel est passé en ready to read
            if (!FD_ISSET(i, &read_fds)) { // n'est pas dedans
                continue;
            }

            if (i == listener) { // on a recu une nouvelle connexion
                if (number_clients >= MAX_NUMBER_CLIENTS) {
                    fprintf(stderr, "nombre maximal de clients atteint\n");
                    continue;
                }
                addrlen = sizeof(remoteaddr);
                newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen);
                if (newfd == -1) {
                    perror("erreur pendant accept");
                    continue;
                }
                FD_SET(newfd, &all_fds);
                number_clients++;
                if (newfd > fdmax) {
                    fdmax = newfd;
                }
                inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, sizeof(remoteIP));
                printf("%s s'est connecté\n", remoteIP);
                printf("nombre de clients : %d\n", number_clients);
            }
            else { // on a recu un message ou un socket s'est déco
                if (recv(i, buf, MAX_SIZE_NAME+MAX_SIZE_MESSAGE, 0) <= 0) {
                    close(i);
                    FD_CLR(i, &all_fds);
                    number_clients--;
                    printf("déconnexion d'un client\n");
                    printf("nombre de clients : %d\n", number_clients);
                }
                else {
                    printf("réception\n");
                    // print_ascii(buf, MAX_SIZE_NAME+MAX_SIZE_MESSAGE);
                    if (strncmp(buf+MAX_SIZE_NAME, "exit", MAX_SIZE_MESSAGE) == 0) {
                        // on a recu "exit" = le client veut se deco
                        // on lui confirme
                        send(i, buf, MAX_SIZE_NAME+MAX_SIZE_MESSAGE, 0);
                        // et on le deco
                        close(i);
                        FD_CLR(i, &all_fds);
                        number_clients--;
                        printf("déconnexion d'un client\n");
                        printf("nombre de clients : %d\n", number_clients);
                    }
                    else {
                        // sinon on envoie le message à tout le monde
                        printf("envoi à tout le monde\n");
                        for (int j = 0; j <= fdmax; i=j++) {
                            if (!FD_ISSET(j, &all_fds) || j == listener) {
                                continue;
                            }
                            send(j, buf, MAX_SIZE_NAME+MAX_SIZE_MESSAGE, 0);
                        }
                    }
                }
            }
        }
    }

    close(listener);

    return 0;
}