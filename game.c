// game.c
#include "game.h"
#include <stdio.h>

void init_game(Game *game) {
    for(int i = 0; i < ROWS; i++)
        for(int j = 0; j < COLS; j++)
            game->board[i][j] = ' ';
    game->current_player = 1;
}

void print_board(Game *game) {
    printf("\n");
    for(int i = 0; i < ROWS; i++) {
        printf("|");
        for(int j = 0; j < COLS; j++) {
            printf(" %c |", game->board[i][j]);
        }
        printf("\n");
    }
    printf("-----------------------------\n");
    printf("  ");
    for(int j = 0; j < COLS; j++) {
        printf(" %d  ", j+1);
    }
    printf("\n\n");
}

int make_move(Game *game, int col) {
    if(col < 0 || col >= COLS) return 0;
    for(int i = ROWS-1; i >=0; i--) {
        if(game->board[i][col] == ' ') {
            game->board[i][col] = (game->current_player == 1) ? 'X' : 'O';
            game->current_player = (game->current_player == 1) ? 2 : 1;
            return 1;
        }
    }
    return 0; // Столбец заполнен или неверный
}

int check_winner(Game *game) {
    // Проверка горизонталей
    for(int i = 0; i < ROWS; i++)
        for(int j = 0; j < COLS-3; j++)
            if(game->board[i][j] != ' ' &&
               game->board[i][j] == game->board[i][j+1] &&
               game->board[i][j] == game->board[i][j+2] &&
               game->board[i][j] == game->board[i][j+3])
                return (game->board[i][j] == 'X') ? 1 : 2;

    // Проверка вертикалей
    for(int i = 0; i < ROWS-3; i++)
        for(int j = 0; j < COLS; j++)
            if(game->board[i][j] != ' ' &&
               game->board[i][j] == game->board[i+1][j] &&
               game->board[i][j] == game->board[i+2][j] &&
               game->board[i][j] == game->board[i+3][j])
                return (game->board[i][j] == 'X') ? 1 : 2;

    // Проверка диагоналей (слева направо)
    for(int i = 0; i < ROWS-3; i++)
        for(int j = 0; j < COLS-3; j++)
            if(game->board[i][j] != ' ' &&
               game->board[i][j] == game->board[i+1][j+1] &&
               game->board[i][j] == game->board[i+2][j+2] &&
               game->board[i][j] == game->board[i+3][j+3])
                return (game->board[i][j] == 'X') ? 1 : 2;

    // Проверка диагоналей (справа налево)
    for(int i = 0; i < ROWS-3; i++)
        for(int j = 3; j < COLS; j++)
            if(game->board[i][j] != ' ' &&
               game->board[i][j] == game->board[i+1][j-1] &&
               game->board[i][j] == game->board[i+2][j-2] &&
               game->board[i][j] == game->board[i+3][j-3])
                return (game->board[i][j] == 'X') ? 1 : 2;

    return 0; // Нет победителя
}
