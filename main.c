#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include<netdb.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

bool add_jednomasztowiec(char board[4][4], const char input[3], char flag) {
    unsigned int row = input[0] - 'A';
    unsigned int col = input[1] - '0' - 1;

    if (board[row][col] == ' ') {
        board[row][col] = flag;
        return true;
    } else {
        printf( "W tym miejscu jest juz statek!\n");
        return false;
    }
}

bool add_dwumasztowiec(char board[4][4], char in1[3], char in2[3]){

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

void print_board(char board[4][4]) {
    size_t i, j;
    char alfabet[4] = {'A', 'B', 'C', 'D'};
    printf("  1 2 3 4\n");
    for (i=0; i<4; ++i) {
        printf("%c ", alfabet[i]);
        for (j=0; j<4; ++j) {
            printf("%c ", board[i][j]);
        }
        printf("\n");
    }
}

int main(int argc, char * argv[]) {
    extern int h_errno;
    struct hostent *host;
    char   **bufs;
    struct in_addr *addr;

    host = gethostbyname(argv[1]);
    if(host == NULL)
    {
        herror("Blad gethostbyname");
        return -1;
    }
    char board[4][4];
    size_t i, j;

    for (i=0; i<4; ++i) for (j=0; j<4; ++j) board[i][j] = ' ';

    char jed1[3];
    char jed2[3];
    char dwu1[3];
    char dwu2[3];


    while (true) {
        printf("1. jednomasztowiec:");
        scanf("%s", jed1);
        if (add_jednomasztowiec(board, jed1 , '1')){
            break;
        } else {
            printf("Podaj dane ponownie\n");
        }
    }
    while (true) {
        printf("2. jednomasztowiec:");
        scanf("%s", jed2);
        if (add_jednomasztowiec(board, jed2 , '1')){
            break;
        } else {
            printf("Podaj dane ponownie\n");
        }
    }
    while (true) {
        printf("3. dwumasztowiec:");
        scanf("%s %s", dwu1, dwu2);
        if (add_dwumasztowiec(board, dwu1, dwu2)){
            break;
        } else {
            printf("Podaj dane ponownie\n");
        }
    }
    print_board(board);

    return 0;
}