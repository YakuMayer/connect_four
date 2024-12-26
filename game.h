#ifndef GAME_H
#define GAME_H

#include <stddef.h> // Для size_t

#define ROWS 6
#define COLUMNS 7

typedef enum {
    EMPTY,
    USER,
    SERVER
} Cell;

typedef enum {
    ONGOING,
    WIN_USER,
    WIN_SERVER,
    DRAW
} GameStatus;

typedef struct {
    Cell board[ROWS][COLUMNS];
    GameStatus status;
} Game;

// Инициализация игры (обнуление поля)
void init_game(Game *game);

// Ставим фишку player (USER, SERVER) в указанный столбец column
// Возвращает -1, если столбец заполнен или недопустим
int drop_piece(Game *game, int column, Cell player);

// Проверка победителя (горизонтали/вертикали/диагонали/ничья)
GameStatus check_winner(Game *game);

// Преобразование поля в JSON-массив, без "current_turn"
void board_to_json(Game *game, char *json, size_t max_length);

// Для отладки (печать поля в консоль)
void print_game_state(Game *game);

#endif // GAME_H
