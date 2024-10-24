#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstring> // pour memset
#include <set>

constexpr int MAX_SIZE_NAME = 20;
constexpr int MAX_SIZE_MESSAGE = 100;
constexpr int MAX_SIZE_BUFFER = MAX_SIZE_NAME+MAX_SIZE_MESSAGE;
constexpr char PORT[] = "3114";
constexpr int MAX_LOG = 10;
constexpr int MAX_CLIENTS = 10;

class ChatServer {
public:
    ChatServer() : m_listener(), m_fd_max(), m_fd_list({STDIN_FILENO}), m_servaddr(), m_all_fds(), m_read_fds()
    {
        FD_ZERO(&m_all_fds);
        FD_ZERO(&m_read_fds);

        FD_SET(STDIN_FILENO, &m_all_fds);
        m_fd_max = STDIN_FILENO;
    }

    int start() {
        int yes = 1;
        struct addrinfo hints, *p;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if (getaddrinfo(nullptr, PORT, &hints, &m_servaddr) != 0) {
            return 2; // pas de errno donc on différencie
        }
        for (p = m_servaddr; p != nullptr; p = p->ai_next) {
            if ((m_listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                continue;
            }

            if (setsockopt(m_listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
                close(m_listener);
                continue;
            }

            if (bind(m_listener, p->ai_addr, p->ai_addrlen) == -1) {
                close(m_listener);
                continue;
            }

            break;
        }
        freeaddrinfo(m_servaddr);
        if (p == nullptr) { // pas trouvé de bonne adresse
            return 3;
        }

        if (listen(m_listener, MAX_LOG) != 0) {
            return 1;
        }
        FD_SET(m_listener, &m_all_fds);
        m_fd_list.insert(m_listener);
        m_fd_max = std::max(m_fd_max, m_listener);

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
            m_read_fds = m_all_fds;

            if (select(m_fd_max+1, &m_read_fds, nullptr, nullptr, nullptr) == -1) {
                return 1;
            }

            for (int fd : m_fd_list) {
                if (!FD_ISSET(fd, &m_read_fds)) {
                    continue;
                }

                if (fd == STDIN_FILENO) {
                    memset(buffer, '\0', MAX_SIZE_BUFFER);
                    fgets(buffer, MAX_SIZE_BUFFER, stdin);

                    if (strncmp(buffer, "exit\n", MAX_SIZE_MESSAGE) == 0) {
                        running = false;
                    }
                }

                else if (fd == m_listener) {
                    if ((remote_fd = accept(m_listener, (struct sockaddr*)&remote_addr, &addrlen)) == -1) {
                        perror("Erreur pendant accept");
                    }

                    if (m_fd_list.size()-1 >= MAX_CLIENTS) {
                        std::cerr << "Nombre maximal de clients atteint\n";
                        break;
                    }

                    FD_SET(remote_fd, &m_all_fds);
                    m_fd_list.insert(remote_fd);
                    m_fd_max = std::max(m_fd_max, remote_fd);
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
                            FD_CLR(fd, &m_all_fds);
                            m_fd_list.erase(fd);
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
                        FD_CLR(fd, &m_all_fds);
                        m_fd_list.erase(fd);
                        std::cout << "Un client s'est déconnecté\n";
                        break;
                    }

                    for (int dest : m_fd_list) {
                        if (dest == STDIN_FILENO || dest == m_listener) {
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
        for (int fd : m_fd_list) {
            close(fd);
        }
    }
private:
    int m_listener;
    int m_fd_max;
    std::set<int> m_fd_list;
    struct addrinfo *m_servaddr;
    fd_set m_all_fds;
    fd_set m_read_fds;
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