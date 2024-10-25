#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <netdb.h>

#include <iostream>
#include <cstring>

constexpr int MAX_SIZE_NAME = 20;
constexpr int MAX_SIZE_MESSAGE = 100;
constexpr int MAX_SIZE_BUFFER = MAX_SIZE_NAME+MAX_SIZE_MESSAGE;
constexpr int MAX_LOG = 10;
constexpr int MAX_CLIENTS = 10;

class ChatClient {
public:
    ChatClient(const char* ip, const char* port)
        : m_remote_fd(), m_remote_ip(), m_port(),
        m_servaddr(), m_all_fds(), m_read_fds()
    {
        FD_ZERO(&m_all_fds);
        FD_ZERO(&m_read_fds);

        FD_SET(STDIN_FILENO, &m_all_fds);

        strncpy(m_remote_ip, ip, INET6_ADDRSTRLEN);
        strncpy(m_port, port, 5);
    }

    int start() {
        struct addrinfo hints, *p;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(m_remote_ip, m_port, &hints, &m_servaddr) != 0) {
            return 2; // pas de errno donc on différencie
        }
        for (p = m_servaddr; p != nullptr; p = p->ai_next) {
            if ((m_remote_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                continue;
            }

            if (connect(m_remote_fd, p->ai_addr, p->ai_addrlen) == -1) {
                close(m_remote_fd);
                continue;
            }

            break;
        }
        freeaddrinfo(m_servaddr);
        if (p == nullptr) { // pas trouvé de bonne adresse
            return 3;
        }

        FD_SET(m_remote_fd, &m_all_fds);

        return 0;
    }

    int run() {
        bool running = true;
        int return_val;
        char buffer[MAX_SIZE_BUFFER];

        memset(buffer, '\0', MAX_SIZE_NAME);
        std::cout << "Entrez votre nom : ";
        fgets(buffer, MAX_SIZE_NAME, stdin);
        buffer[strlen(buffer)-1] = '\0'; // enlève le \n

        while (running) {
            m_read_fds = m_all_fds;

            if (select(m_remote_fd+1, &m_read_fds, nullptr, nullptr, nullptr) == -1) {
                return 1;
            }

            for (int fd = 0; fd <= m_remote_fd; ++fd) {
                if (!FD_ISSET(fd, &m_read_fds)) {
                    continue;
                }

                if (fd == STDIN_FILENO) {
                    memset(buffer+MAX_SIZE_NAME, '\0', MAX_SIZE_MESSAGE);
                    fgets(buffer+MAX_SIZE_NAME, MAX_SIZE_MESSAGE, stdin);
                    buffer[MAX_SIZE_NAME+strlen(buffer+MAX_SIZE_NAME)-1] = '\0'; // enlève le \n

                    if (send(m_remote_fd, buffer, MAX_SIZE_BUFFER, 0) == -1) {
                        perror("Erreur send");
                    }
                }

                else if (fd == m_remote_fd) {
                    memset(buffer, '\0', MAX_SIZE_BUFFER);
                    if ((return_val = recv(fd, buffer, MAX_SIZE_BUFFER, 0)) <= 0) {
                        if (return_val == -1) {
                            perror("Erreur pendant réception");
                            break;
                        }
                        if (return_val == 0) {
                            running = false;
                            close(fd);
                            std::cout << "Le serveur a fermé la connexion\n";
                            break;
                        }
                    }

                    if (strncmp(buffer+MAX_SIZE_NAME, "exit", MAX_SIZE_MESSAGE) == 0) {
                        running = false;
                        std::cout << "Déconnexion\n";
                        break;
                    }

                    std::cout << buffer << " : " << buffer+MAX_SIZE_NAME << "\n";
                }
            }
        }

        return 0;
    }

    virtual ~ChatClient() {
        close(m_remote_fd);
    }
private:
    int m_remote_fd;
    char m_remote_ip[INET6_ADDRSTRLEN];
    char m_port[5];
    struct addrinfo *m_servaddr;
    fd_set m_all_fds;
    fd_set m_read_fds;
};

int main(int argc, char* argv[]) {

    if (argc != 3) {
        std::cerr << "Usage : ./client <server_ip> <port>\n";
        exit(1);
    }

    {
        ChatClient client(argv[1], argv[2]);
        int return_val;

        if ((return_val = client.start()) != 0) {
            if (return_val == 1) {
                perror("Erreur pendant le démarrage");
            }
            else {
                std::cout << "Erreur pendant le démarrage\n"; 
            }
            exit(return_val);
        }
        std::cout << "Client lancé\n";

        if ((return_val = client.run()) != 0) {
            if (return_val == 1) {
                perror("Erreur pendant la boucle principale");
            }
            else {
                std::cout << "Erreur pendant la boucle principale\n"; 
            }
            exit(return_val);
        }
        std::cout << "Fermeture du client\n";
    }

    return 0;
}