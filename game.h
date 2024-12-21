#ifndef GAME_H
#define GAME_H

#define ROWS 6
#define COLS 7

typedef struct {
    char board[ROWS][COLS];
    int current_player;
} Game;

void init_game(Game *game);
int make_move(Game *game, int col);
const char *serialize_board(Game *game);

#endif // GAME_H
