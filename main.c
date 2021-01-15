#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/signal.h>

/* Struktura pomocnicza do wysyłania */
struct message {
    char nick[32];
    char msg[256];
};

struct addrinfo *addr;
int sockfd;

/* sighandler */
void handler(int sig) {
    freeaddrinfo(addr);
    close(sockfd);
    exit(EXIT_SUCCESS);
}

/* error handler */
void exit_with_error(const char *err) {
    perror(err);
    freeaddrinfo(addr);
    close(sockfd);
    exit(EXIT_FAILURE);
}

bool add_jednomasztowiec(char board[4][4], const char input[3], char flag) {
    unsigned int row = input[0] - 'A';
    unsigned int col = input[1] - '0' - 1;

    if (board[row][col] == ' ') {
        board[row][col] = flag;
        return true;
    } else {
        printf("W tym miejscu jest juz statek!\n");
        return false;
    }
}

bool add_dwumasztowiec(char board[4][4], char in1[3], char in2[3]) {

    int row1 = in1[0] - 'A';
    int col1 = in1[1] - '0' - 1;
    int row2 = in2[0] - 'A';
    int col2 = in2[1] - '0' - 1;

    if (abs(row1 - row2) <= 1 && abs(col1 - col2) <= 1 && (row1 == row2 || col1 == col2)) {
        if (!add_jednomasztowiec(board, in1, '2')) return false;
        if (!add_jednomasztowiec(board, in2, '2')) {
            board[row1][col1] = ' ';
            return false;
        }
        return true;
    } else {
        printf("Pozycje dwumasztowca muszą być obok siebie\n");
        return false;
    }
}

/* Zwraca 3 wartości:
 * 0 = Nie trafiony, 1 = Trafiony i zatopiony, 2 = Trafiony, ale nie zatopiony  */
int hit(char board[4][4], char hitboard[4][4], const char in[3]) {
    int row = in[0] - 'A';
    int col = in[1] - '0' - 1;
    if (board[row][col] == '1') {
        hitboard[row][col] = 'Z';
        return 1;
    } else if (board[row][col] == '2') {
        int dwu = 0;
        /* sprawdza istnienie pozostałej części dwumasztowca */
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (board[i][j] == '2') {
                    ++dwu;
                }
            }
        }
        if (dwu < 2) {
            hitboard[row][col] = 'Z';
            return 1;
        } else {
            hitboard[row][col] = 'x';
            return 2;
        }
    } else {
        return 0;
    }
}

/* wypisuje planszę w odpowiednim formacie */
void print_board(char board[4][4]) {
    char alfabet[4] = {'A', 'B', 'C', 'D'};
    printf("  1 2 3 4\n");
    for (int i = 0; i < 4; ++i) {
        printf("%c ", alfabet[i]);
        for (int j = 0; j < 4; ++j) {
            printf("%c ", board[i][j]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {

    struct sockaddr_in client_addr, server_addr;
    struct message message;
    int bindresult;
    ssize_t bytes;

    struct addrinfo hints;
    struct addrinfo *p;

    /* Zerowanie stuktury hints */
    memset(&hints, 0, sizeof(hints));

    /* IPV4 i UDP */
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    /* ustawiam port */
    int n = 18000;
    char port[7];
    sprintf(port, "%d", n);

    /* ustawienie hosta dla argv[1] */
    if (getaddrinfo(argv[1], port, &hints, &addr) != 0) {
        perror("getaddrinfo");
        exit(EXIT_FAILURE);
    }
    /* przechodzimy pętlę ze wszystkimi skojarzonymi adresami
     * i po znalezieniu pierwszego właściwego opuszczamy pętle*/
    for (p = addr; p != NULL; p = p->ai_next) {
        server_addr = *((struct sockaddr_in *) p->ai_addr);
        break;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        freeaddrinfo(addr);
        exit(EXIT_FAILURE);
    }

    /* Konfiguracja adresu lokalnego */
    /* IPv4 */
    client_addr.sin_family = AF_INET;
    /* Dowolny interfejs */
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* Port */
    client_addr.sin_port = htons(n);
    /* skojarzenie gniazda z adresem lokalnym */
    bindresult = bind(sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr));
    if (bindresult == -1)
        exit_with_error("Bind");

    printf("Rozpoczynam czat z %s. Napisz <koniec> by zakonczyc czat."
           " Ustal polozenie swoich okretow:\n", inet_ntoa(server_addr.sin_addr));

    /* zmienne i wywołania potrzebne do gry w statki
     * nie działają jednak w tej wersji programu */
    /* plansza zawierająca statki w pozycjach wybranych przez użytkownika*/
    char board[4][4];
    /* plansza z ozaczeniami trafień */
    char hitboard[4][4];
    /* jednomasztowce */
    char jed1[3];
    char jed2[3];
    /* dwumasztowce */
    char dwu1[3];
    char dwu2[3];

    /* wypełnienie obu planszy */
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            board[i][j] = ' ';
            hitboard[i][j] = ' ';
        }
    }
     /* wejście do danych */
    while (true) {
        printf("1. jednomasztowiec:");
        scanf("%s", jed1);
        if (add_jednomasztowiec(board, jed1, '1')) {
            break;
        } else {
            printf("Podaj dane ponownie\n");
        }
    }
    while (true) {
        printf("2. jednomasztowiec:");
        scanf("%s", jed2);
        if (add_jednomasztowiec(board, jed2, '1')) {
            break;
        } else {
            printf("Podaj dane ponownie\n");
        }
    }
    while (true) {
        printf("3. dwumasztowiec:");
        scanf("%s %s", dwu1, dwu2);
        if (add_dwumasztowiec(board, dwu1, dwu2)) {
            break;
        } else {
            printf("Podaj dane ponownie\n");
        }
    }
    printf("Plansza z własnymi statkami\n");
    print_board(board);
    printf("Plansza z trafieniami\n");
    print_board(hitboard);

    /* Ustawianie nicku na bazie argv[2], jeśli nie ma to ustawia "NN" */
    char *name = argc == 2 ? "NN" : argv[2];
    strcpy(message.nick, name);
    /* Do obsługi zakończenia child process */
    signal(SIGCHLD, handler);
    /* Wiadomość oznaczająca nawiązanie połączenia */
    strcpy(message.msg, "!@#$%^&^%$#@!");
    bytes = sendto(sockfd, &message, sizeof(message), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes == -1) {
        exit_with_error("Brak połączenia");
    }

    bytes = -1;

    /* child process do obsługi wysyłania */
    int pid;
    if ((pid = fork()) == 0) {
        while (true) {
            printf("[%s]> ", name);
            /* pobieramy wiadomość od użytkownika */
            fgets(message.msg, 255, stdin);
            message.msg[strlen(message.msg) - 1] = '\0';

            /* wysyłamy wiadomość */
            bytes = sendto(sockfd, &message, sizeof(message), 0, (struct sockaddr *) &server_addr,
                           sizeof(server_addr));
            if (bytes == -1) {
                exit_with_error("Nie udalo sie wysłać wiadomości");
            }

            bytes = -1;

            if (strcmp(message.msg, "<koniec>") == 0) {
                exit(EXIT_SUCCESS);
            }
        }

    } else if (pid != -1) {
        while (true) {
            unsigned int nn = sizeof(server_addr);
            /* Odbieranie wiadomości */
            bytes = recvfrom(sockfd, &message, sizeof(message), 0, (struct sockaddr *) &server_addr, &nn);
            if (bytes == -1)
                exit_with_error("Blad recvfrom");

            bytes = -1;

            /* Komunikat zależny od wiadomości */
            if (strcmp(message.msg, "<koniec>") == 0) {
                printf("[%s (%s) zakonczyl rozmowe]\n", message.nick, inet_ntoa(server_addr.sin_addr));
                freeaddrinfo(addr);
                close(sockfd);
                fflush(stdout);
                kill(pid, SIGKILL);
                exit(EXIT_SUCCESS);
            } else if (strcmp(message.msg, "!@#$%^&^%$#@!") == 0) {
                printf("\n[%s (%s) dolaczyl do rozmowy]\n", message.nick, inet_ntoa(server_addr.sin_addr));
            } else {
                printf("[%s (%s)]: %s\n", message.nick, inet_ntoa(server_addr.sin_addr), message.msg);
            }

            fflush(stdout);
        }
    } else {
        exit_with_error("Błąd fork");
    }
    return 0;
}