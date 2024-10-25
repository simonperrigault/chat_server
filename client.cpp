#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

constexpr int MAX_SIZE_NAME = 20;
constexpr int MAX_SIZE_MESSAGE = 100;
constexpr int MAX_SIZE_BUFFER = MAX_SIZE_NAME+MAX_SIZE_MESSAGE;
constexpr int MAX_LOG = 10;
constexpr int MAX_CLIENTS = 10;

void sauter_ligne_curseur(WINDOW* window) {
    int x,y;
    getyx(window, y, x);
    wmove(window, y+1, 0);
    wrefresh(window);
}

class ChatClient {
public:
    ChatClient(WINDOW* window, const char* ip, const char* port)
        : m_remote_fd(), m_remote_ip(), m_port(),
        m_servaddr(), m_all_fds(), m_read_fds(), m_old_messages(), m_max_old_messages(),
        m_window(window), m_messages_window(), m_input_window()
    {
        FD_ZERO(&m_all_fds);
        FD_ZERO(&m_read_fds);

        FD_SET(STDIN_FILENO, &m_all_fds);

        strncpy(m_remote_ip, ip, INET6_ADDRSTRLEN);
        strncpy(m_port, port, 5);

        int rows, cols;
        getmaxyx(m_window, rows, cols);
        m_messages_window = newwin(rows-3, cols, 0, 0); // -3 pour les bordures des fenêtres
        m_input_window = newwin(3, cols, rows-3, 0);
        m_max_old_messages = rows-5;
    }

    int start() {
        struct addrinfo hints, *p;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(m_remote_ip, m_port, &hints, &m_servaddr) != 0) {
            return 2;
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
        char buffer_recv[MAX_SIZE_BUFFER];
        char buffer_send[MAX_SIZE_BUFFER];
        int curr_char;

        memset(buffer_send, '\0', MAX_SIZE_BUFFER);
        ajouter_message("Entrez votre nom : ");
        draw_messages();
        draw_input("");
        while ((curr_char = wgetch(m_input_window)) != '\n') {
            if (curr_char == KEY_BACKSPACE || curr_char == 127 || curr_char == '\b') {
                if (strlen(buffer_send) > 0) {
                    buffer_send[strlen(buffer_send)-1] = '\0';
                }
            }
            else if (strlen(buffer_send) >= MAX_SIZE_NAME-1) {
                continue;
            }
            else {
                buffer_send[strlen(buffer_send)] = curr_char;
                draw_input(buffer_send);
            }
        }
        buffer_send[MAX_SIZE_NAME-1] = '\0';

        flushinp();
        m_old_messages.clear();
        draw_messages();
        draw_input("");

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
                    curr_char = wgetch(m_input_window);
                    if (curr_char == KEY_BACKSPACE || curr_char == 127 || curr_char == '\b') {
                        if (strlen(buffer_send+MAX_SIZE_NAME) > 0) {
                            buffer_send[MAX_SIZE_NAME + strlen(buffer_send+MAX_SIZE_NAME)-1] = '\0';
                        }
                    }
                    else if (curr_char == '\n') {
                        if (send(m_remote_fd, buffer_send, MAX_SIZE_BUFFER, 0) == -1) {
                            ajouter_message("Erreur pendant l'envoi");
                        }
                        memset(buffer_send+MAX_SIZE_NAME, '\0', MAX_SIZE_MESSAGE);
                    }
                    else if (strlen(buffer_send+MAX_SIZE_NAME) >= MAX_SIZE_MESSAGE-1) {
                        continue;
                    }
                    else {
                        buffer_send[MAX_SIZE_NAME + strlen(buffer_send+MAX_SIZE_NAME)] = curr_char;
                    }
                }

                else if (fd == m_remote_fd) {
                    memset(buffer_recv, '\0', MAX_SIZE_BUFFER);
                    if ((return_val = recv(fd, buffer_recv, MAX_SIZE_BUFFER, 0)) <= 0) {
                        if (return_val == -1) {
                            ajouter_message("Erreur pendant la réception");
                            break;
                        }
                        if (return_val == 0) {
                            running = false;
                            close(fd);
                            ajouter_message("Le serveur a fermé la connexion");
                            break;
                        }
                    }

                    if (strncmp(buffer_recv+MAX_SIZE_NAME, "exit", MAX_SIZE_MESSAGE) == 0) {
                        running = false;
                        ajouter_message("Déconnexion");
                        break;
                    }

                    char temp[MAX_SIZE_BUFFER+3] = {'\0'};
                    strncpy(temp, buffer_recv, MAX_SIZE_NAME);
                    strcat(temp, " : ");
                    strncat(temp, buffer_recv+MAX_SIZE_NAME, MAX_SIZE_MESSAGE);
                    ajouter_message(temp);
                }
            }
            draw_messages();
            draw_input(buffer_send+MAX_SIZE_NAME);
        }

        return 0;
    }

    virtual ~ChatClient() {
        wprintw(m_window, "Fermeture du client");
        sauter_ligne_curseur(m_window);
        close(m_remote_fd);
    }
private:
    int m_remote_fd;
    char m_remote_ip[INET6_ADDRSTRLEN];
    char m_port[5];
    struct addrinfo *m_servaddr;
    fd_set m_all_fds;
    fd_set m_read_fds;
    std::vector<std::string> m_old_messages;
    unsigned int m_max_old_messages;
    WINDOW* m_window;
    WINDOW* m_messages_window;
    WINDOW* m_input_window;

    void draw_messages() {
        werase(m_messages_window);
        box(m_messages_window, 0, 0);
        for (unsigned int i = 0; i < m_old_messages.size(); i++) {
            mvwprintw(m_messages_window, 1+i, 1, "%s", m_old_messages[i].c_str());
        }
        wrefresh(m_messages_window);
    }
    void draw_input(const char* message) {
        werase(m_input_window);
        box(m_input_window, 0, 0);
        mvwprintw(m_input_window, 1, 1, "%s", message);
        wrefresh(m_input_window);
    }
    void ajouter_message(const char* message) {
        if (m_old_messages.size() >= m_max_old_messages) {
            m_old_messages.erase(m_old_messages.begin());
        }
        m_old_messages.emplace_back(message);
    }
};

int main(int argc, char* argv[]) {
    int return_val = 0;
    WINDOW* window = initscr();
    noecho();
    cbreak();
    // curs_set(0);

    if (argc != 3) {
        wprintw(window, "Usage : ./client <server_ip> <port>");
        sauter_ligne_curseur(window);
        exit(1);
    }

    {
        ChatClient client(window, argv[1], argv[2]);

        if ((return_val = client.start()) != 0) {
            wprintw(window, "Erreur pendant le démarrage : %s", strerror(errno));
            sauter_ligne_curseur(window);
        }
        else {
            if ((return_val = client.run()) != 0) {
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