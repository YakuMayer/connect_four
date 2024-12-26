// game.c
#include "game.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void init_game(Game *game) {
    for(int i = 0; i < ROWS; i++) {
        for(int j = 0; j < COLUMNS; j++) {
            game->board[i][j] = EMPTY;
        }
    }
    game->status = ONGOING;
}

// Ставим фишку player (USER или SERVER) в указанный столбец column
int drop_piece(Game *game, int column, Cell player) {
    if(column < 0 || column >= COLUMNS)
        return -1;
    for(int i = ROWS - 1; i >= 0; i--) {
        if(game->board[i][column] == EMPTY) {
            game->board[i][column] = player;
            return i; 
        }
    }
    return -1; // столбец заполнен
}

// Проверка победителя
GameStatus check_winner(Game *game) {
    // Горизонтали
    for(int i = 0; i < ROWS; i++) {
        for(int j = 0; j <= COLUMNS - 4; j++) {
            Cell c = game->board[i][j];
            if(c != EMPTY &&
               c == game->board[i][j+1] &&
               c == game->board[i][j+2] &&
               c == game->board[i][j+3]) {
                return (c == USER) ? WIN_USER : WIN_SERVER;
            }
        }
    }

    // Вертикали
    for(int i = 0; i <= ROWS - 4; i++) {
        for(int j = 0; j < COLUMNS; j++) {
            Cell c = game->board[i][j];
            if(c != EMPTY &&
               c == game->board[i+1][j] &&
               c == game->board[i+2][j] &&
               c == game->board[i+3][j]) {
                return (c == USER) ? WIN_USER : WIN_SERVER;
            }
        }
    }

    // Диагонали (слева-направо)
    for(int i = 0; i <= ROWS - 4; i++) {
        for(int j = 0; j <= COLUMNS - 4; j++) {
            Cell c = game->board[i][j];
            if(c != EMPTY &&
               c == game->board[i+1][j+1] &&
               c == game->board[i+2][j+2] &&
               c == game->board[i+3][j+3]) {
                return (c == USER) ? WIN_USER : WIN_SERVER;
            }
        }
    }

    // Диагонали (справа-налево)
    for(int i = 0; i <= ROWS - 4; i++) {
        for(int j = 3; j < COLUMNS; j++) {
            Cell c = game->board[i][j];
            if(c != EMPTY &&
               c == game->board[i+1][j-1] &&
               c == game->board[i+2][j-2] &&
               c == game->board[i+3][j-3]) {
                return (c == USER) ? WIN_USER : WIN_SERVER;
            }
        }
    }

    // Ничья?
    int filled = 1;
    for(int i = 0; i < ROWS; i++) {
        for(int j = 0; j < COLUMNS; j++) {
            if(game->board[i][j] == EMPTY) {
                filled = 0;
                break;
            }
        }
        if(!filled) break;
    }
    if(filled)
        return DRAW;

    return ONGOING;
}

// Преобразование поля в JSON-массив
void board_to_json(Game *game, char *json, size_t max_length) {
    snprintf(json, max_length, "[");
    for(int i = 0; i < ROWS; i++) {
        strncat(json, "[", max_length - strlen(json) - 1);
        for(int j = 0; j < COLUMNS; j++) {
            if(game->board[i][j] == EMPTY) {
                strncat(json, "\" \"", max_length - strlen(json) - 1);
            }
            else if(game->board[i][j] == USER) {
                strncat(json, "\"X\"", max_length - strlen(json) - 1);
            }
            else {
                strncat(json, "\"O\"", max_length - strlen(json) - 1);
            }
            if(j < COLUMNS - 1) {
                strncat(json, ", ", max_length - strlen(json) - 1);
            }
        }
        strncat(json, "]", max_length - strlen(json) - 1);
        if(i < ROWS - 1) {
            strncat(json, ", ", max_length - strlen(json) - 1);
        }
    }
    strncat(json, "]", max_length - strlen(json) - 1);
}

void print_game_state(Game *game) {
    for(int i = 0; i < ROWS; i++) {
        for(int j = 0; j < COLUMNS; j++) {
            char c;
            if(game->board[i][j] == EMPTY)       c = '.';
            else if(game->board[i][j] == USER)   c = 'X';
            else                                 c = 'O';
            printf("%c ", c);
        }
        printf("\n");
    }
    printf("Status: ");
    switch(game->status) {
        case ONGOING:    printf("Ongoing\n"); break;
        case WIN_USER:   printf("User Wins\n"); break;
        case WIN_SERVER: printf("Server Wins\n"); break;
        case DRAW:       printf("Draw\n");      break;
    }
    printf("\n");
}
