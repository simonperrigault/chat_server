#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <cstring> // for memset
#include <set>

constexpr int MAX_SIZE_NAME = 20;
constexpr int MAX_SIZE_MESSAGE = 100;
constexpr int MAX_SIZE_BUFFER = MAX_SIZE_NAME+MAX_SIZE_MESSAGE;
constexpr char PORT[] = "3114";
constexpr int MAX_LOG = 10;
constexpr int MAX_CLIENTS = 10;

class ChatServer {
public:
    ChatServer() : listener(), fd_max(), fd_list({STDIN_FILENO}), servaddr(), all_fds(), read_fds()
    {
        FD_ZERO(&all_fds);
        FD_ZERO(&read_fds);

        FD_SET(STDIN_FILENO, &all_fds);
        fd_max = STDIN_FILENO;
    }

    int start() {
        int yes = 1;
        struct addrinfo hints, *p;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if (getaddrinfo(nullptr, PORT, &hints, &servaddr) != 0) {
            return 2; // pas de errno donc on différencie
        }
        for (p = servaddr; p != nullptr; p = p->ai_next) {
            if ((listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                continue;
            }

            if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
                close(listener);
                continue;
            }

            if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) {
                close(listener);
                continue;
            }

            break;
        }
        freeaddrinfo(servaddr);
        if (p == nullptr) { // pas trouvé de bonne adresse
            return 3;
        }

        if (listen(listener, MAX_LOG) != 0) {
            return 1;
        }
        FD_SET(listener, &all_fds);
        fd_list.insert(listener);
        fd_max = std::max(fd_max, listener);

        return 0;
    }

    int run() {
        bool running = true;
        int return_val;
        int remote_fd;
        char buffer[MAX_SIZE_BUFFER];
        struct sockaddr_storage remote_addr;
        socklen_t addrlen;

        while (running) {
            read_fds = all_fds;

            if (select(fd_max+1, &read_fds, nullptr, nullptr, nullptr) == -1) {
                return 1;
            }

            for (int fd : fd_list) {
                if (!FD_ISSET(fd, &read_fds)) {
                    continue;
                }

                if (fd == STDIN_FILENO) {
                    memset(buffer, '\0', MAX_SIZE_BUFFER);
                    fgets(buffer, MAX_SIZE_BUFFER, stdin);

                    if (strncmp(buffer, "exit\n", MAX_SIZE_MESSAGE) == 0) {
                        running = false;
                    }
                }

                else if (fd == listener) {
                    if ((remote_fd = accept(listener, (struct sockaddr*)&remote_addr, &addrlen)) == -1) {
                        perror("Erreur pendant accept");
                    }

                    if (fd_list.size()-1 >= MAX_CLIENTS) {
                        std::cerr << "Nombre maximal de clients atteint\n";
                        break;
                    }

                    FD_SET(remote_fd, &all_fds);
                    fd_list.insert(remote_fd);
                    fd_max = std::max(fd_max, remote_fd);
                    std::cout << "Nouvelle connexion\n";
                }

                else {
                    memset(buffer, '\0', MAX_SIZE_BUFFER);
                    if ((return_val = recv(fd, buffer, MAX_SIZE_BUFFER, 0)) <= 0) {
                        if (return_val == -1) {
                            perror("Erreur pendant réception");
                            break;
                        }
                        if (return_val == 0) {
                            close(fd);
                            FD_CLR(fd, &all_fds);
                            fd_list.erase(fd);
                            std::cout << "Un client s'est déconnecté\n";
                            break;
                        }
                    }

                    std::cout << "Réception d'un message\n";

                    if (strncmp(buffer+MAX_SIZE_NAME, "exit", MAX_SIZE_MESSAGE) == 0) {
                        if (send(fd, buffer, MAX_SIZE_BUFFER, 0) == -1) {
                            perror("Erreur pendant send");
                        }
                        close(fd);
                        FD_CLR(fd, &all_fds);
                        fd_list.erase(fd);
                        std::cout << "Un client s'est déconnecté\n";
                        break;
                    }

                    for (int dest : fd_list) {
                        if (dest == STDIN_FILENO || dest == listener) {
                            continue;
                        }

                        if (send(dest, buffer, MAX_SIZE_BUFFER, 0) == -1) {
                            perror("Erreur pendant envoi");
                        }
                    }
                }
            }
        }

        return 0;
    }

    virtual ~ChatServer() {
        for (int fd : fd_list) {
            close(fd);
        }
    }
private:
    int listener;
    int fd_max;
    std::set<int> fd_list;
    struct addrinfo *servaddr;
    fd_set all_fds;
    fd_set read_fds;
};

int main() {

    {
        ChatServer server;
        int return_val;

        if ((return_val = server.start()) != 0) {
            if (return_val == 1) {
                perror("Erreur pendant le démarrage");
            }
            else {
                std::cout << "Erreur pendant le démarrage\n"; 
            }
            exit(return_val);
        }
        std::cout << "Serveur lancé\n";

        if ((return_val = server.run()) != 0) {
            if (return_val == 1) {
                perror("Erreur pendant la boucle principale");
            }
            else {
                std::cout << "Erreur pendant la boucle principale\n"; 
            }
            exit(return_val);
        }
        std::cout << "Fermeture du serveur\n";
    }

    return 0;
}