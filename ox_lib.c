#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <stdbool.h>
#include <time.h>
#include "ox_hdr.h"

OXGamer *create_oxgamer(char *name, int sockfd){
    OXGamer *gamer = malloc(sizeof(OXGamer));
    strcpy(gamer->name, name);
    gamer->sockfd = sockfd;
    gamer->status = FREE;
    gamer->ox_noti_idx = 0;

    for(size_t i = 0; i < 64; ++i)
        gamer->ox_noti[i] = NULL;

    return gamer;
}

OXGame *create_oxgame(int sockfd1, int sockfd2){
    OXGame *new_game = malloc(sizeof(OXGame));
    for(int i = 0; i < 3; ++i)
        for(int j = 0; j < 3; ++j)
            new_game->board[i][j] = ' ';
    new_game->id = sockfd1;
    new_game->turn = sockfd1;
    new_game->status = PLAYING;
    new_game->left_step = 9;
    new_game->gamer_fd[0] = sockfd1;
    new_game->gamer_fd[1] = sockfd2;
    new_game->gamer_label[0] = 'o';
    new_game->gamer_label[1] = 'x';

    return new_game;
}

OXNoti *create_oxnoti(char *initiator_name, int initiator_fd){
    OXNoti *new_noti = malloc(sizeof(OXNoti));
    strcpy(new_noti->name, initiator_name);
    new_noti->sockfd = initiator_fd;

    return new_noti;
}

_Bool __is_diagonal_win(char (*board)[3]){
    if((board[0][0] == 'o' && board[1][1] == 'o' && board[2][2] == 'o') ||
        (board[0][0] == 'x' && board[1][1] == 'x' && board[2][2] == 'x') ||
        (board[0][2] == 'o' && board[1][1] == 'o' && board[2][0] == 'o') ||
        (board[0][2] == 'x' && board[1][1] == 'x' && board[2][0] == 'x'))
        return true;
    return false;
}

_Bool __is_vertical_win(char (*board)[3]){
    if((board[0][0] == 'o' && board[1][0] == 'o' && board[2][0] == 'o') ||
        (board[0][1] == 'o' && board[1][1] == 'o' && board[2][1] == 'o') || 
        (board[0][2] == 'o' && board[1][2] == 'o' && board[2][2] == 'o') ||
        (board[0][0] == 'x' && board[1][0] == 'x' && board[2][0] == 'x') ||
        (board[0][1] == 'x' && board[1][1] == 'x' && board[2][1] == 'x') ||
        (board[0][2] == 'x' && board[1][2] == 'x' && board[2][2] == 'x'))
        return true;
    return false;
}

_Bool __is_horizontal_win(char (*board)[3]){
    if((board[0][0] == 'o' && board[0][1] == 'o' && board[0][2] == 'o') ||
        (board[1][0] == 'o' && board[1][1] == 'o' && board[1][2] == 'o') || 
        (board[2][0] == 'o' && board[2][1] == 'o' && board[2][2] == 'o') ||
        (board[0][0] == 'x' && board[0][1] == 'x' && board[0][2] == 'x') ||
        (board[1][0] == 'x' && board[1][1] == 'x' && board[1][2] == 'x') || 
        (board[2][0] == 'x' && board[2][1] == 'x' && board[2][2] == 'x'))
        return true;
    return false;
}

_Bool is_win(OXGame *ox_game){
    if(__is_horizontal_win(ox_game->board) ||
        __is_vertical_win(ox_game->board) ||
        __is_diagonal_win(ox_game->board))
        return true;
    return false;
}

_Bool is_draw(OXGame *ox_game){
    return (!is_win(ox_game) && ox_game->left_step == 0);
}
