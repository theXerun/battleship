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
#include <ctype.h>
#include <sys/mman.h>
#include <sys/shm.h>

/* makra dla lepszej czytelności */
#define miss 0
#define hit_and_killed_jednomasztowiec 1
#define hit_not_killed_dwumasztowiec 2
#define hit_and_killed_dwumasztowiec 3
#define win 4
#define end 5
#define connected 10
#define aim 11

/* Struktura pomocnicza do wysyłania */
struct message {
    char nick[32]; // nick wyświetlany
    char shot[3]; // koordynaty strzału
    int reaction; // reakcja jedna z makr powyżej albo -1
    int killcount; // liczy zestrzelenia
};

struct addrinfo *addr;
int sockfd;
/* pamięć dzielona */
int shmid;
struct message *shmptr;

/* sighandler */
void handler(int sig) {
    freeaddrinfo(addr);
    close(sockfd);
    shmdt(shmptr);
    shmctl(shmid, IPC_RMID, NULL);
    exit(EXIT_SUCCESS);
}

/* error handler */
void exit_with_error(const char *err) {
    perror(err);
    freeaddrinfo(addr);
    close(sockfd);
    shmdt(shmptr);
    shmctl(shmid, IPC_RMID, NULL);
    exit(EXIT_FAILURE);
}

/* dodaje jednomasztowiec na wybraną pozycję
 * przyjmuje też 'flag' w celu ponownego użycia funkcji dla większych statków */
bool add_jednomasztowiec(char board[4][4], const char input[3], char flag) {

    int row = input[0] - 'A';
    int col = input[1] - '0' - 1;

    /* jeśli miejsce jest puste to oznacza je jako statek
     * w przeciwnym wypadku daje informację zwrotną */
    if (board[row][col] == ' ') {
        board[row][col] = flag;
        return true;
    } else {
        printf("W tym miejscu jest juz statek!\n");
        return false;
    }
}

/* dodaje dwumasztowiec na wybraną pozycję
 * uprzednio też sprawdza czy ta pozycja jest legalna,
 * nie zajęta, obok siebie i nie na ukos */
bool add_dwumasztowiec(char board[4][4], char in1[3], char in2[3]) {

    int row1 = in1[0] - 'A';
    int col1 = in1[1] - '0' - 1;
    int row2 = in2[0] - 'A';
    int col2 = in2[1] - '0' - 1;

    /* sprawdzanie legalności pozycji */
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

/* wymusza zamianę znaku na podanej planszy na flag */
void force_replace(char board[4][4], const char input[3], char flag) {
    unsigned int row = input[0] - 'A';
    unsigned int col = input[1] - '0' - 1;
    board[row][col] = flag;
}

/* Zwraca 3 wartości:
 * 0 = Nie trafiony, 1 = Trafiony i zatopiony, 2 = Trafiony, ale nie zatopiony  */
int hit(char board[4][4], const char in[3]) {
    int row = in[0] - 'A';
    int col = in[1] - '0' - 1;
    if (board[row][col] == '1') {
        return hit_and_killed_jednomasztowiec;
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
            return hit_and_killed_dwumasztowiec;
        } else {
            return hit_not_killed_dwumasztowiec;
        }
    } else {
        return miss;
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

/* czyści planszę poprzez zamianę każdego znaku na whitespace */
void clear_board(char board[4][4]) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            board[i][j] = ' ';
        }
    }
}

/* pobiera od użytkownika dane */
void populate_board(char board[4][4]) {

    /* jednomasztowce */
    char jed1[3];
    char jed2[3];
    /* dwumasztowiec */
    char dwu1[3];
    char dwu2[3];

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
}

/* sprawdza czy podane dane są koordynatami w odpowiednim formacie np. A2 */
bool is_coords(const char *msg) {
    if (isalpha(msg[0]) && (msg[0] == 'A' || msg[0] == 'B' || msg[0] == 'C' || msg[0] == 'D')) {
        if (isdigit(msg[1]) && (msg[1] == '1' || msg[1] == '2' || msg[1] == '3' || msg[1] == '4')) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

int main(int argc, char *argv[]) {

    struct sockaddr_in client_addr, server_addr;
    struct message player, opponent;
    char msg[256];
    volatile bool missed;
    int bindresult;
    struct addrinfo hints;
    struct addrinfo *p;
    ssize_t bytes;

    /* tworzę pamięć dzieloną dla procesów parent/child */
    shmid = shmget(IPC_PRIVATE, sizeof(struct message), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget error\n");
        exit(EXIT_FAILURE);
    }
    shmptr = (struct message *) shmat(shmid, NULL, 0);

    /* Zerowanie stuktury hints */
    memset(&hints, 0, sizeof(hints));

    /* IPV4 i UDP */
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    /* ustawiam port */
    int n = 18000;
    char port[6];
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
    /* deskryptor gniazda */
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


    /* plansza zawierająca statki w pozycjach wybranych przez użytkownika*/
    char board[4][4];
    /* plansza z ozaczeniami trafień */
    char hitboard[4][4];
    /* tymczasowa zmienna do przetrzymywania lokalizacji trafienia dwumasztowca */
    char hitdwu[3];

    /* wypełnienie obu planszy pustymi znakami */
    clear_board(board);
    clear_board(hitboard);

    /* wejście do danych */
    populate_board(board);

    /* Ustawianie nicku na bazie argv[2], jeśli nie ma to ustawia "NN" */
    char *name = argc == 2 ? "NN" : argv[2];
    strcpy(player.nick, name);

    /* Do obsługi zakończenia child process */
    signal(SIGCHLD, handler);

    /* Wiadomość oznaczająca nawiązanie połączenia */
    player.reaction = connected;
    bytes = sendto(sockfd, &player, sizeof(player),
                   0, (struct sockaddr *) &server_addr, sizeof(server_addr));

    if (bytes == -1) {
        exit_with_error("Brak połączenia");
    }
    printf("[Propozycja gry wyslana]\n");

    bytes = -1;
    player.reaction = -1; // domyślna reakcja

    /* pamięć dzielona */
    char defaultshot[3] = {' ', ' ', '\0'};
    strncpy(shmptr->shot, defaultshot, 3);
    shmptr->killcount = 0;

    /* child process do obsługi wysyłania */
    int pid;
    if ((pid = fork()) == 0) {
        while (true) {
            if (shmptr->killcount > 2) {
                player.reaction = win;
                printf("[%s (%s) wygrales]\n", opponent.nick,
                       inet_ntoa(server_addr.sin_addr));
                msg[0] = '\0';
            } else {
                player.shot[0] = ' ';
                player.shot[1] = ' ';
                /* pobieramy wiadomość od użytkownika */
                fgets(msg, 255, stdin);
                msg[strlen(msg) - 1] = '\0';
            }

            if (strcmp(msg, "wypisz") == 0) {
                print_board(hitboard);

            } else if (strcmp(msg, "<koniec>") == 0) {
                player.reaction = end;
                sendto(sockfd, &player, sizeof(player), 0,
                       (struct sockaddr *) &server_addr,
                       sizeof(server_addr));
                freeaddrinfo(addr);
                close(sockfd);
                shmdt(shmptr);
                shmctl(shmid, IPC_RMID, NULL);
                exit(EXIT_SUCCESS);

            } else if (is_coords(msg)) {
                player.shot[0] = msg[0];
                player.shot[1] = msg[1];
                player.shot[2] = '\0';
                player.reaction = aim;
                strncpy(shmptr->shot, player.shot, 3);
            } else {
                player.reaction = -1;
            }


            /* wysyłanie wiadomości */
            bytes = sendto(sockfd, &player, sizeof(player),
                           0, (struct sockaddr *) &server_addr,
                           sizeof(server_addr));
            if (bytes == -1) {
                exit_with_error("Nie udalo sie wysłać wiadomości");
            }
            bytes = -1;

        }
        //proces główny
    } else if (pid != -1) {
        while (true) {
            /* Odbieranie wiadomości */
            unsigned int nn = sizeof(server_addr);
            bytes = recvfrom(sockfd, &opponent, sizeof(opponent),
                             0, (struct sockaddr *) &server_addr, &nn);
            if (bytes == -1) {
                exit_with_error("Blad recvfrom");
            }
            bytes = -1;

            /* wykonuje polecenie gdy reakcja jest inna niż domyślna =-1 */
            if (opponent.reaction != -1) {
                /* kopiuje zawartość shmptr zmodyfikowaną przez child do lokalnego player.shot */
                strncpy(player.shot, shmptr->shot, 3);

                /* wiadomość przywitania */
                if (opponent.reaction == connected) {
                    printf("[%s (%s): dolaczyl do gry, podaj pole do strzalu]\n",
                           opponent.nick, inet_ntoa(server_addr.sin_addr));

                /* nadpisuje zmienną miss jeśli przeciwnik da znać że się nie trafiło
                 * potrzebne do wiadomości typu "[Pudlo, " */
                } else if (opponent.reaction == miss) {
                    missed = true;

                /* reakcja na trafienie i zatopienie jednomasztowca */
                } else if (opponent.reaction == hit_and_killed_jednomasztowiec) {
                    force_replace(hitboard, player.shot, 'Z');
                    ++shmptr->killcount;
                    printf("[%s (%s): zatopiles jednomasztowiec, podaj pole do strzalu]\n",
                           opponent.nick, inet_ntoa(server_addr.sin_addr));

                /* reakcja na trafienie dwumasztowca */
                } else if (opponent.reaction == hit_not_killed_dwumasztowiec) {
                    force_replace(hitboard, player.shot, 'x');
                    strcpy(hitdwu, player.shot); // chwilowa zmienna do trzymania
                    printf("[%s (%s): trafiles dwumasztowiec, podaj kolejne pole]\n",
                           opponent.nick, inet_ntoa(server_addr.sin_addr));

                /* reakcja na trafienie i zatopienie dwumasztowca */
                } else if (opponent.reaction == hit_and_killed_dwumasztowiec) {
                    printf("[%s (%s): zatopiles dwumasztowiec, podaj kolejne pole]\n",
                           opponent.nick, inet_ntoa(server_addr.sin_addr));
                    force_replace(hitboard, player.shot, 'Z');
                    force_replace(hitboard, hitdwu, 'Z');
                    player.shot[0] = ' ';
                    player.shot[1] = ' ';
                    ++shmptr->killcount;

                /* reakcja na zwycięztwo przeciwnika */
                } else if (opponent.reaction == win) {
                    printf("[%s (%s) wygral, przegrales]\n",
                           opponent.nick, inet_ntoa(server_addr.sin_addr));

                /* reakcja na zakończenie gry przez przeciwnika */
                } else if (opponent.reaction == end) {
                    getc(stdin);
                    printf("[%s (%s) zakonczyl gre, czy chcesz przygotowac nowa plansze? (t/n)]\n",
                           opponent.nick, inet_ntoa(server_addr.sin_addr));
                    scanf("%c", &msg[0]);
                    if (msg[0] == 't') {
                        clear_board(board);
                        clear_board(hitboard);
                        populate_board(board);
                        player.reaction = connected;
                    } else {
                        freeaddrinfo(addr);
                        close(sockfd);
                        shmdt(shmptr);
                        shmctl(shmid, IPC_RMID, NULL);
                        kill(pid, SIGINT);
                        exit(EXIT_SUCCESS);
                    }

                /* ta reakcja sygnalizuje, że przeciwnik wybrał cel i strzelił,
                 * pod tym ifem są reakcje na wszystkie możliwości a następnie wysyłana jest
                 * odpowiedź odnośnie trafienia */
                } else if (opponent.reaction == aim && is_coords(opponent.shot)) {
                    player.reaction = hit(board, opponent.shot);
                    /* formatowanie z missed */
                    if (missed == false) {
                        printf("[");
                    } else {
                        printf("[Pudlo, ");
                        missed = false;
                    }
                    /*reakcja na nietrafienie */
                    if (player.reaction == miss) {
                        printf("%s (%s) strzela %s pudlo, podaj pole do strzalu]\n",
                               opponent.nick, inet_ntoa(server_addr.sin_addr), opponent.shot);

                    /*reakcja na trafienie i zatopienie jednomasztowca */
                    } else if (player.reaction == hit_and_killed_jednomasztowiec) {
                        force_replace(board, opponent.shot, ' ');
                        printf("%s (%s) strzela %s - jednomasztowiec trafiony]\n",
                               opponent.nick, inet_ntoa(server_addr.sin_addr), opponent.shot);

                    /*reakcja na trafienie (ale nie zatopienie) dwumasztowca */
                    } else if (player.reaction == hit_not_killed_dwumasztowiec) {
                        force_replace(board, opponent.shot, ' ');
                        printf("%s (%s) strzela %s - dwumasztowiec trafiony]\n",
                               opponent.nick, inet_ntoa(server_addr.sin_addr), opponent.shot);

                    /*reakcja na trafienie i zatopienie dwumasztowca */
                    } else if (player.reaction == hit_and_killed_dwumasztowiec) {
                        force_replace(board, opponent.shot, ' ');
                        printf("%s (%s) strzela %s - dwumasztowiec zatopiony]\n",
                               opponent.nick, inet_ntoa(server_addr.sin_addr), opponent.shot);
                    } else {
                        player.reaction = -1;
                    }
                    /* reset strzału */
                    opponent.shot[0] = ' ';
                    opponent.shot[1] = ' ';
                    /* wysyła reakcję z player.reaction */
                    sendto(sockfd, &player, sizeof(player), 0, (struct sockaddr *) &server_addr,
                           sizeof(server_addr));
                }

            }
            fflush(stdout);
        }
    } else {
        exit_with_error("Błąd fork");
    }
    return 0;
}