#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include <cstring>
#include <set>

constexpr int MAX_SIZE_NAME = 20;
constexpr int MAX_SIZE_MESSAGE = 100;
constexpr int MAX_SIZE_BUFFER = MAX_SIZE_NAME+MAX_SIZE_MESSAGE;
constexpr char PORT[] = "3114";
constexpr int MAX_LOG = 10;
constexpr int MAX_CLIENTS = 10;

void sauter_ligne_curseur(WINDOW* window) {
    int x,y;
    getyx(window, y, x);
    wmove(window, y+1, 0);
    wrefresh(window);
}

class ChatServer {
public:
    ChatServer(WINDOW* window)
        : m_listener(), m_fd_max(), m_fd_list({STDIN_FILENO}), 
        m_servaddr(), m_all_fds(), m_read_fds(), m_window(window)
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

        wprintw(m_window, "Serveur connecté");
        sauter_ligne_curseur(m_window);

        return 0;
    }

    int run() {
        bool running = true;
        int return_val;
        int remote_fd;
        char buffer_message[MAX_SIZE_BUFFER];
        int curr_input;
        struct sockaddr_storage remote_addr;
        socklen_t addrlen;

        wprintw(m_window, "Serveur lancé");
        sauter_ligne_curseur(m_window);
        wprintw(m_window, "Appuyez sur q pour arrêter...");
        sauter_ligne_curseur(m_window);

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
                    curr_input = wgetch(m_window);

                    if (curr_input == 'q') {
                        running = false;
                    }
                }

                else if (fd == m_listener) {
                    if ((remote_fd = accept(m_listener, (struct sockaddr*)&remote_addr, &addrlen)) == -1) {
                        wprintw(m_window, "Erreur pendant accept");
                        sauter_ligne_curseur(m_window);
                    }

                    if (m_fd_list.size()-1 >= MAX_CLIENTS) {
                        wprintw(m_window, "Nombre maximal de clients atteint");
                        sauter_ligne_curseur(m_window);
                        break;
                    }

                    FD_SET(remote_fd, &m_all_fds);
                    m_fd_list.insert(remote_fd);
                    m_fd_max = std::max(m_fd_max, remote_fd);
                    wprintw(m_window, "Un client s'est connecté");
                    sauter_ligne_curseur(m_window);
                }

                else {
                    memset(buffer_message, '\0', MAX_SIZE_BUFFER);
                    if ((return_val = recv(fd, buffer_message, MAX_SIZE_BUFFER, 0)) <= 0) {
                        if (return_val == -1) {
                            wprintw(m_window, "Erreur pendant la réception");
                            sauter_ligne_curseur(m_window);
                        }
                        if (return_val == 0) {
                            close(fd);
                            FD_CLR(fd, &m_all_fds);
                            m_fd_list.erase(fd);
                            wprintw(m_window, "Un client s'est déconnecté");
                            sauter_ligne_curseur(m_window);
                            break;
                        }
                    }

                    if (strncmp(buffer_message+MAX_SIZE_NAME, "exit", MAX_SIZE_MESSAGE) == 0) {
                        if (send(fd, buffer_message, MAX_SIZE_BUFFER, 0) == -1) {
                            wprintw(m_window, "Erreur pendant envoi");
                            sauter_ligne_curseur(m_window);
                        }
                        close(fd);
                        FD_CLR(fd, &m_all_fds);
                        m_fd_list.erase(fd);
                        wprintw(m_window, "Un client s'est déconnecté");
                        sauter_ligne_curseur(m_window);
                        break;
                    }

                    wprintw(m_window, "Réception d'un message");
                    sauter_ligne_curseur(m_window);
                    for (int dest : m_fd_list) {
                        if (dest == STDIN_FILENO || dest == m_listener) {
                            continue;
                        }

                        if (send(dest, buffer_message, MAX_SIZE_BUFFER, 0) == -1) {
                            wprintw(m_window, "Erreur pendant envoi");
                            sauter_ligne_curseur(m_window);
                        }
                    }
                }
            }
        }

        return 0;
    }

    virtual ~ChatServer() {
        wprintw(m_window, "Fermeture du serveur");
        sauter_ligne_curseur(m_window);
        for (int fd : m_fd_list) {
            if (fd == STDIN_FILENO) {
                continue;
            }
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
    WINDOW* m_window;
};

int main() {
    int return_val = 0;
    WINDOW* window = initscr();
    noecho();
    cbreak();
    curs_set(0);

    {
        ChatServer server(window);

        if ((return_val = server.start()) != 0) {
            wprintw(window, "Erreur pendant le démarrage : %s", strerror(errno));
            sauter_ligne_curseur(window);
        }
        else {
            if ((return_val = server.run()) != 0) {
                wprintw(window, "Erreur pendant la boucle principale : %s", strerror(errno));
                sauter_ligne_curseur(window);
            }
        }
    }

    wprintw(window, "Appuyez sur une touche pour continuer...");
    sauter_ligne_curseur(window);
    wgetch(window);

    endwin();

    return return_val;
}