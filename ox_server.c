#include "basis.h"
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include "ox_hdr.h"

SOCKET create_socket(const char* host, const char *port) {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(host, port, &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return socket_listen;
}



#define MAX_REQUEST_SIZE 2047

struct client_info {
    socklen_t address_length;
    struct sockaddr_storage address;
    SOCKET socket;
    char request[MAX_REQUEST_SIZE + 1];
    int received;
    struct client_info *next;
};

static struct client_info *clients = 0;

struct client_info *get_client(SOCKET s) {
    struct client_info *ci = clients;

    while(ci) {
        if (ci->socket == s)
            break;
        ci = ci->next;
    }

    if (ci) return ci;
    struct client_info *n =
        (struct client_info*) calloc(1, sizeof(struct client_info));

    if (!n) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }

    n->address_length = sizeof(n->address);
    n->next = clients;
    clients = n;
    return n;
}


void drop_client(struct client_info *client) {
    CLOSESOCKET(client->socket);

    struct client_info **p = &clients;

    while(*p) {
        if (*p == client) {
            *p = client->next;
            free(client);
            return;
        }
        p = &(*p)->next;
    }

    fprintf(stderr, "drop_client not found.\n");
    exit(1);
}


const char *get_client_address(struct client_info *ci) {
    static char address_buffer[100];
    getnameinfo((struct sockaddr*)&ci->address,
            ci->address_length,
            address_buffer, sizeof(address_buffer), 0, 0,
            NI_NUMERICHOST);
    return address_buffer;
}




fd_set wait_on_clients(SOCKET server) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(server, &reads);
    SOCKET max_socket = server;

    struct client_info *ci = clients;

    while(ci) {
        FD_SET(ci->socket, &reads);
        if (ci->socket > max_socket)
            max_socket = ci->socket;
        ci = ci->next;
    }

    if (select(max_socket+1, &reads, 0, 0, 0) < 0) {
        fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return reads;
}

#define gamer_idx 1024
#define game_idx 1024
OXGamer *login_gamers[1024];
OXGame *running_games[1024];
// size_t gamer_idx = 0;
// size_t game_idx = 0;

int main() {

    SOCKET server = create_socket(0, "8000");

    while(1) {

        fd_set reads;
        reads = wait_on_clients(server);

        if (FD_ISSET(server, &reads)) {
            struct client_info *client = get_client(-1);

            client->socket = accept(server,
                    (struct sockaddr*) &(client->address),
                    &(client->address_length));

            if (!ISVALIDSOCKET(client->socket)) {
                fprintf(stderr, "accept() failed. (%d)\n",
                        GETSOCKETERRNO());
                return 1;
            }


            printf("New connection from %s.\n",
                    get_client_address(client));
        }


        struct client_info *client = clients;
        while(client) {
            struct client_info *next = client->next;

            if (FD_ISSET(client->socket, &reads)) {

                if (MAX_REQUEST_SIZE == client->received) {
                    client = next;
                    continue;
                }

                int r = recv(client->socket,
                        client->request + client->received,
                        MAX_REQUEST_SIZE - client->received, 0);

                if (r < 1) {
                    printf("Unexpected disconnect from %s.\n",
                            get_client_address(client));
                    drop_client(client);

                } else {
                    client->received += r;
                    client->request[client->received] = 0;

                    _Bool is_exit_req;
                    is_exit_req = req_parser(client->request, client->socket);
                    if(is_exit_req == false){
                        del_gamer(client->socket);
                        printf("bye!\n");
                        drop_client(client);
                    } else {
                        memset(client->request, 0, sizeof(char[MAX_REQUEST_SIZE + 1]));
                        client->received = 0;
                    }
                }
            }

            client = next;
        }

    } //while(1)


    printf("\nClosing socket...\n");
    CLOSESOCKET(server);

    printf("Finished.\n");
    return 0;
}

char *get_name_by_sockfd(int sockfd){
    OXGamer *gamer;
    for(size_t i = 0; i < gamer_idx; ++i){
        gamer = login_gamers[i];
        if(gamer && gamer->sockfd == sockfd){
            char *ret = malloc(64);
            memset(ret, 0, 64);
            strcpy(ret, gamer->name);
            return ret;
        }
    }
    return NULL;
}

_Bool req_parser(char *cmd, int initiator_fd){
    char *ptr;
    char buf[1024] = {0};
    // printf("cmd: %s\n", cmd);
    if((ptr = strstr(cmd, "lsgamers"))){
        // show all login gamers
        strcpy(buf, "login gamers:\n");
        OXGamer *gamer;
        _Bool has_gamer = false;

        for(size_t i = 0; i < gamer_idx; ++i){
            gamer = login_gamers[i];
            if(gamer && gamer->sockfd != initiator_fd){
                has_gamer = true;

                strcat(buf, gamer->name);
                char id_buf[8];
                sprintf(id_buf, "%d", gamer->sockfd);
                strcat(buf, "@");
                strcat(buf, id_buf);
                strcat(buf, "\n");
            }
        }

        if(!has_gamer)
            strcat(buf, "empty\n");

        send(initiator_fd, buf, strlen(buf), 0);
        return true;
    } else if((ptr = strstr(cmd, "lsgames"))){
        // show all running games
        strcpy(buf, "running games:\n");
        OXGame *game;
        _Bool has_game = false;

        for(size_t i = 0; i < game_idx; ++i){
            game = running_games[i];
            if(game){
                has_game = true;
                // gamer 1
                char *gamer_name = get_name_by_sockfd(game->gamer_fd[0]);
                strcat(buf, gamer_name);
                char id_buf[8];
                sprintf(id_buf, "%d", game->gamer_fd[0]);
                strcat(buf, "@");
                strcat(buf, id_buf);
                strcat(buf, " vs ");
                free(gamer_name);

                // gamer 2
                gamer_name = get_name_by_sockfd(game->gamer_fd[1]);
                strcat(buf, gamer_name);
                sprintf(id_buf, "%d", game->gamer_fd[1]);
                strcat(buf, "@");
                strcat(buf, id_buf);
                strcat(buf, " ");
                free(gamer_name);

                char game_id[8];
                sprintf(game_id, "%d", game->id);
                strcat(buf, "[game_id: ");
                strcat(buf, game_id);
                strcat(buf, "]");
                strcat(buf, "\n");
            }
        }

        if(!has_game)
            strcat(buf, "empty\n");

        send(initiator_fd, buf, strlen(buf), 0);
        return true;
    } else if((ptr = strstr(cmd, "lsnoti"))){
        strcpy(buf, "notifications:\n");
        _Bool has_noti = false;

        OXGamer *gamer;
        for(size_t i = 0; i < gamer_idx; ++i){
            gamer = login_gamers[i];
            if(gamer && gamer->sockfd == initiator_fd){
                gamer = login_gamers[i];
                break;
            }
        }

        OXNoti *noti;
        for(size_t i = 0; i < gamer->ox_noti_idx; ++i){
            noti = gamer->ox_noti[i];
            if(noti){
                has_noti = true;

                strcat(buf, noti->name);
                char id_buf[8];
                sprintf(id_buf, "%d", noti->sockfd);
                strcat(buf, "@");
                strcat(buf, id_buf);
                strcat(buf, "\n");
            }
        }

        if(!has_noti)
            strcat(buf, "empty\n");

        send(initiator_fd, buf, strlen(buf), 0);
        return true;
    } else if((ptr = strstr(cmd, "watchgame "))){
        ptr += 10;
        char game_id_buf[8] = {0};
        char *newline = strchr(ptr, '\n');
        snprintf(game_id_buf, newline - ptr + 1, "%s", ptr);
        int game_id = atoi(game_id_buf);

        // check game if exists
        OXGame *game = get_game_by_sockfd(game_id);
        if(!game){
            const char err_msg[] = "no such games!(enter 'lsgames' to list all running games)\n";
            send(initiator_fd, err_msg, strlen(err_msg), 0);
            return true;
        }

        // check the initiator if is in the game
        OXGame *_game = get_game_by_sockfd(initiator_fd);
        if(_game){
            const char err_msg[] = "you need to leave or finish the current game first!\n";
            send(initiator_fd, err_msg, strlen(err_msg), 0);
            return true;
        }

        game->watchers[game->watchers_idx++] = initiator_fd;

        // draw ox_board to initiator
        draw_oxboard_to_watcher(initiator_fd, game);        

        return true;
    } else if((ptr = strstr(cmd, "exitgame"))){

        // delete the game if the gamer is in the game, also the gamer then notify the peer
        OXGame *game = get_game_by_sockfd(initiator_fd);
        if(game){
            int peer_fd;
            if(game->gamer_fd[0] == initiator_fd)
                peer_fd = game->gamer_fd[1];
            else
                peer_fd = game->gamer_fd[0];

            send(peer_fd, "Peer has exit the game!\n", strlen("Peer has exit the game!\n"), 0);

            if(game->watchers_idx > 0){
                char exit_msg[128];
                if(initiator_fd == game->gamer_fd[0])
                    strcpy(exit_msg, "'o' side has exit the game\n");
                else
                    strcpy(exit_msg, "'x' side has exit the game\n");

                for(int i = 0; i < game->watchers_idx; ++i)
                    send(game->watchers[i], exit_msg, strlen(exit_msg), 0);
            }

            del_game(game);
            del_gamer(initiator_fd);
        } 

        send(initiator_fd, "exitg", 5, 0);

        return false;
    } else if((ptr = strstr(cmd, "leavegame"))){
        // delete the game if the initiator is in the game
        OXGame *game = get_game_by_sockfd(initiator_fd);
        if(!game){
            // check the initiator if is watching a game
            for(int i = 0; i < game_idx; ++i){
                game = running_games[i];
                if(game){
                    for(int j = 0; j < game->watchers_idx; ++j){
                        if(game->watchers[j] == initiator_fd){
                            game->watchers[j] = -1;
                            game->watchers_idx--;
                            const char msg[] = "leave watching game successfully\n";
                            send(initiator_fd, msg, strlen(msg), 0);
                            return true;
                        }
                    }
                }
            }

            const char msg[] = "you are not playing or watching any game!\n";
            send(initiator_fd, msg, strlen(msg), 0);
            return true;
        }

        int peer_fd;
        if(game->gamer_fd[0] == initiator_fd)
            peer_fd = game->gamer_fd[1];
        else
            peer_fd = game->gamer_fd[0];

        send(peer_fd, "Peer has leave the game!\n", strlen("Peer has leave the game!\n"), 0);

        if(game->watchers_idx > 0){
            char leave_msg[128];
            if(initiator_fd == game->gamer_fd[0])
                strcpy(leave_msg, "'o' side has exit the game\n");
            else
                strcpy(leave_msg, "'x' side has exit the game\n");

            for(int i = 0; i < game->watchers_idx; ++i)
                send(game->watchers[i], leave_msg, strlen(leave_msg), 0);
        }
        del_game(game);

        const char msg[] = "leave game successfully\n";
        send(initiator_fd, msg, strlen(msg), 0);

        return true;
    } else if((ptr = strstr(cmd, "logout"))){

        OXGame *game = get_game_by_sockfd(initiator_fd);
        if(game){
            int peer_fd;
            if(game->gamer_fd[0] == initiator_fd)
                peer_fd = game->gamer_fd[1];
            else
                peer_fd = game->gamer_fd[0];

            send(peer_fd, "Peer has exit the game!\n", strlen("Peer has exit the game!\n"), 0);

            if(game->watchers_idx > 0){
                char exit_msg[128];
                if(initiator_fd == game->gamer_fd[0])
                    strcpy(exit_msg, "'o' side has exit the game\n");
                else
                    strcpy(exit_msg, "'x' side has exit the game\n");

                for(int i = 0; i < game->watchers_idx; ++i)
                    send(game->watchers[i], exit_msg, strlen(exit_msg), 0);
            }

            del_game(game);
        } 

        del_gamer(initiator_fd);
        send(initiator_fd, "logout", 6, 0);

        return false;
    } else if((ptr = strstr(cmd, "invgamer "))){
        ptr += 9;
        char tgt_fd_buf[8] = {0};
        char *newline = strchr(ptr, '\n');
        snprintf(tgt_fd_buf, newline - ptr + 1, "%s", ptr);
        int tgt_fd = atoi(tgt_fd_buf);

        // cannot invite yourselves
        if(tgt_fd == initiator_fd){
            send(initiator_fd, "cannot invite yourselves\n", strlen("cannot invite yourselves\n"), 0);
            return true;
        }

        // check gamer status
        OXGamer *gamer;
        for(size_t i = 0; i < gamer_idx; ++i){
            gamer = login_gamers[i];
            if(gamer && gamer->sockfd == tgt_fd){
                // put invitation to inv_fd buffer
                add_invitation(initiator_fd, tgt_fd);

                send(initiator_fd, "Invitation has sent!\n", strlen("Invitation has sent!\n"), 0);
                return true;
            }
        }

        send(initiator_fd, "no such gamer(enter 'lsgamers' to list all gamers)\n", strlen("no such gamer(enter 'lsgamers' to list all gamers)\n"), 0);

        return true;
    } else if((ptr = strstr(cmd, "play "))){
        // check if the initiator has join a game
        OXGame *game = get_game_by_sockfd(initiator_fd);
        if(!game){
            const char err_msg[] = "you did not join any game!(enter 'lsnoti' to list all notification or 'invgamer' to invite a gamer)\n";
            send(initiator_fd, err_msg, strlen(err_msg), 0);
            return true;
        }

        // check turn first before updating the oxboard's status
        if(game->turn != initiator_fd){
            const char err_msg[] = "Now is not your turn, please wait peer!\n";
            send(initiator_fd, err_msg, strlen(err_msg), 0);
            return true;
        }

        ptr += 5;
        char action[8] = {0};
        char *newline = strchr(ptr, '\n');
        snprintf(action, newline - ptr + 1, "%s", ptr);
        int action_num = atoi(action);

        _Bool update_success = update_oxboard(initiator_fd, action_num);
        if(!update_success){
            printf("update failed!\n");
            draw_oxboard(initiator_fd, 1, "invalid action");
        } else {
            printf("update successfly!\n");

            OXGame *game = get_game_by_sockfd(initiator_fd);
            if(is_win(game)){
                if(game->turn == game->id)
                    draw_oxboard(initiator_fd, 1, "The winner is 'x'");
                else
                    draw_oxboard(initiator_fd, 1, "The winner is 'o'");
                del_game(game);
            } else if(is_draw(game)){
                draw_oxboard(initiator_fd, 1, "draw");
                del_game(game);
            } else {
                draw_oxboard(initiator_fd, 0);
            }
        }
        return true;
    } else if((ptr = strstr(cmd, "accept "))){
        // draw chess board to both gamer
        ptr += 7;
        char inv_fd_buf[8] = {0};
        char *newline = strchr(ptr, '\n');
        snprintf(inv_fd_buf, newline - ptr + 1, "%s", ptr);
        int inv_fd = atoi(inv_fd_buf);

        // check the initiator if is watching a game
        OXGame *game;
        for(int i = 0; i < game_idx; ++i){
            game = running_games[i];
            if(game){
                for(int j = 0; j < game->watchers_idx; ++j){
                    if(game->watchers[j] == initiator_fd){
                        const char err_msg[] = "you need to leave watching the current game first!\n";
                        send(initiator_fd, err_msg, strlen(err_msg), 0);
                        return true;
                    }
                }
            }
        }

        // check invite fd is in the game
        game = get_game_by_sockfd(inv_fd);
        if(game){
            const char err_msg[] = "Peer is in a game!(Enter 'lsgames' and 'watchgame gameID' to watch the game)\n";
            send(initiator_fd, err_msg, strlen(err_msg), 0);
            return true;
        }

        OXNoti *noti = get_noti_by_sockfd(initiator_fd, inv_fd);
        if(noti){
            send(initiator_fd, "game start!\n", strlen("game start!\n"), 0);
            del_invitation(inv_fd, initiator_fd);
            add_game(inv_fd, initiator_fd);
            draw_oxboard(initiator_fd, 0);
        } else {
            send(initiator_fd, "no such notification(enter 'lsnoti' to list all notifications)\n", strlen("no such notification(enter 'lsnoti' to list all notifications)\n"), 0);
        }

        return true;
    } else if((ptr = strstr(cmd, "reject "))){
        // check notification is exists
        ptr += 7;
        char inv_fd_buf[8] = {0};
        char *newline = strchr(ptr, '\n');
        snprintf(inv_fd_buf, newline - ptr + 1, "%s", ptr);
        int inv_fd = atoi(inv_fd_buf);

        OXNoti *noti = get_noti_by_sockfd(initiator_fd, inv_fd);
        if(noti){
            send(initiator_fd, "reject the notification!\n", strlen("reject the invitation!\n"), 0);
            del_invitation(inv_fd, initiator_fd);
        } else {
            send(initiator_fd, "no such notification!(enter 'lsnoti' to list all notification)\n", strlen("no such notification!(enter 'lsnoti' to list all notification)\n"), 0);
        }
        return true;
    } else if((ptr = strstr(cmd, "login"))){
        // check username is exist in db
        ptr += 5;
        char rec[32] = {0};
        char *space = strchr(ptr, ' ');
        memcpy(rec, ptr, space - ptr);

        char db_rec[32];
        size_t db_rec_len;
        FILE *shadow_fptr = fopen("./shadow", "r");
        while(fgets(db_rec, 32, shadow_fptr)){
            db_rec_len = strlen(db_rec);
            if(db_rec_len && db_rec[db_rec_len - 1] == '\n')
                db_rec[db_rec_len - 1] = 0; 

            if(strcmp(db_rec, rec) == 0){
                fclose(shadow_fptr);
                send(initiator_fd, "success", 7, 0);

                // record the login gamer
                char name[11] = {0};
                ptr = rec;
                char *colon = strchr(ptr, ':');
                memcpy(name, ptr, colon - ptr);

                add_gamer(name, initiator_fd);

                return true;
            }
        }

        fclose(shadow_fptr);
        send(initiator_fd, "failure", 7, 0);
        return true;
    }
}

void add_gamer(char *name, int sockfd){
    OXGamer *new_gamer = create_oxgamer(name, sockfd);
    OXGamer *slot;
    for(int i = 0; i < gamer_idx; ++i){
        slot = login_gamers[i];
        if(!slot){
            login_gamers[i] = new_gamer;
            return;
        }
    }
    // login_gamers[gamer_idx++] = new_gamer;
}

void del_gamer(int sockfd){
    OXGamer *gamer;
    for(size_t i = 0; i < gamer_idx; ++i){
        gamer = login_gamers[i];
        if(gamer && gamer->sockfd == sockfd){
            login_gamers[i] = NULL;
            free(gamer);
            return;
        }
    }
}

void add_invitation(int inv_fd, int sockfd){
    OXNoti *noti = get_noti_by_sockfd(sockfd, inv_fd);
    if(noti)
        return;

    char *name = get_name_by_sockfd(inv_fd);
    OXNoti *noti_new = malloc(sizeof(OXNoti));
    strcpy(noti_new->name, name);
    noti_new->sockfd = inv_fd;

    OXGamer *gamer;
    for(size_t i = 0; i < gamer_idx; ++i){
        gamer = login_gamers[i];
        if(gamer && gamer->sockfd == sockfd){
            gamer->ox_noti[gamer->ox_noti_idx++] = noti_new;
            return;
        }
    }

    free(name);
}

void del_invitation(int inv_fd, int sockfd){
    OXGamer *gamer;
    for(size_t i = 0; i < gamer_idx; ++i){
        gamer = login_gamers[i];
        if(gamer && gamer->sockfd == sockfd){
            OXNoti *noti;
            for(int i = 0; i < gamer->ox_noti_idx; ++i){
                noti = gamer->ox_noti[i];
                if(noti && noti->sockfd == inv_fd){
                    free(noti);
                    gamer->ox_noti[i] = NULL;
                    return;
                }
            }
        }
    }
}

void add_game(int inv_fd, int accept_fd){
    OXGame *new_game = create_oxgame(inv_fd, accept_fd);
    OXGame *slot;
    for(int i = 0; i < game_idx; ++i){
        slot = running_games[i];
        if(!slot){
            running_games[i] = new_game;
            return;
        }
    }
    // running_games[game_idx++] = new_game;
}

void del_game(OXGame *game){
    OXGame *oxgame;
    for(size_t i = 0; i < game_idx; ++i){
        oxgame = running_games[i];
        if(oxgame && (oxgame->id == game->id)){
            running_games[i] = NULL;
            free(oxgame);
            return;
        }
    }
    //running_games[game_idx--];
}

void draw_oxboard(int sockfd, int msg_cnt, ...){
    OXGame *oxgame = get_game_by_sockfd(sockfd);
    // printf("sockfd[0]: %d\n", oxgame->gamer_fd[0]);
    // printf("sockfd[1]: %d\n", oxgame->gamer_fd[1]);

    char buf[1024] = {0};
    char turn_buf[1024] = {0};
    char watch_buf[1024] = {0};

    sprintf(buf, "\n%c|%c|%c\n------\n%c|%c|%c\n------\n%c|%c|%c\n"
        "Waiting for peer's action...\n",
        oxgame->board[0][0],oxgame->board[0][1],oxgame->board[0][2],
        oxgame->board[1][0],oxgame->board[1][1],oxgame->board[1][2],
        oxgame->board[2][0],oxgame->board[2][1],oxgame->board[2][2]);

    sprintf(turn_buf, "\n%c|%c|%c\n------\n%c|%c|%c\n------\n%c|%c|%c\n"
        "Now is your turn(enter 'play 1 ... 9' to play)\n",
        oxgame->board[0][0],oxgame->board[0][1],oxgame->board[0][2],
        oxgame->board[1][0],oxgame->board[1][1],oxgame->board[1][2],
        oxgame->board[2][0],oxgame->board[2][1],oxgame->board[2][2]);

    sprintf(watch_buf, "\n%c|%c|%c\n------\n%c|%c|%c\n------\n%c|%c|%c\n",
        oxgame->board[0][0],oxgame->board[0][1],oxgame->board[0][2],
        oxgame->board[1][0],oxgame->board[1][1],oxgame->board[1][2],
        oxgame->board[2][0],oxgame->board[2][1],oxgame->board[2][2]);

    if(msg_cnt){
        for(int i = 0; i < msg_cnt; ++i){
            va_list args;
            va_start(args, msg_cnt);
            for(int i = 0; i < msg_cnt; ++i){
                char *msg = va_arg(args, char *);
                if(strstr(msg , "winner")){ // has winner
                    strcat(turn_buf, "You are LOSE!\n");
                    strcat(buf, "You are WIN!\n");
                    if(oxgame->turn == oxgame->gamer_fd[0])
                        strcat(watch_buf, "'x' is the winner\n");
                    else
                        strcat(watch_buf, "'o' is the winner\n");
                } else if(strstr(msg, "draw")){ // draw
                    strcat(turn_buf, "The game is DRAW!\n");
                    strcat(buf, "The game is DRAW!\n");
                    strcat(watch_buf, "The game is DRAW!\n");
                } else if(strstr(msg, "invalid")){ // invalid action
                    strcat(turn_buf, "invalid action, please try another one\n");
                    strcat(buf, "peer choose an invalid action, please be patient\n");
                    if(oxgame->turn == oxgame->gamer_fd[0])
                        strcat(watch_buf, "'o' side choose an invalid action\n");
                    else
                        strcat(watch_buf, "'x' side choose an invalid action\n");
                }
            }
            va_end(args);
        }
    }

    if(oxgame->watchers_idx > 0){
        if(!msg_cnt){
            if(oxgame->turn == oxgame->gamer_fd[0])
                strcat(watch_buf, "The turn is 'o'\n");
            else
                strcat(watch_buf, "The turn is 'x'\n");
        }

        for(int i = 0; i < oxgame->watchers_idx; ++i)
            send(oxgame->watchers[i], watch_buf, strlen(watch_buf), 0);
    }

    if(oxgame->turn == oxgame->gamer_fd[0]){
        send(oxgame->gamer_fd[0], turn_buf, strlen(turn_buf), 0);
        send(oxgame->gamer_fd[1], buf, strlen(buf), 0);
    } else {
        send(oxgame->gamer_fd[1], turn_buf, strlen(turn_buf), 0);
        send(oxgame->gamer_fd[0], buf, strlen(buf), 0);
    }

    return;
}

_Bool update_oxboard(int sockfd, int action){
    // OXGame *game;
    // for(int i = 0; i < game_idx; ++i){
    //     game = running_games[i];
    //     if(game)
    //         printf("i: %d, game_fd[0]: %d, game_fd[1]: %d\n", i, game->gamer_fd[0], game->gamer_fd[1]);
    // }

    OXGame *oxgame = get_game_by_sockfd(sockfd);
    assert(oxgame != NULL);

    printf("action: %d\n", action);

    if(action == 1){
        if(oxgame->board[0][0] != ' '){
            goto invalid_action;
        } else {
            if(sockfd == oxgame->id){
                oxgame->board[0][0] = oxgame->gamer_label[0];
                oxgame->turn = oxgame->gamer_fd[1];
            } else {
                oxgame->board[0][0] = oxgame->gamer_label[1];
                oxgame->turn = oxgame->gamer_fd[0];
            }
            oxgame->left_step--;

            return true;
        }
    } else if(action == 2){
        if(oxgame->board[0][1] != ' '){
            goto invalid_action;
        } else {
            if(sockfd == oxgame->id){
                oxgame->board[0][1] = oxgame->gamer_label[0];
                oxgame->turn = oxgame->gamer_fd[1];
            }
            else {
                oxgame->board[0][1] = oxgame->gamer_label[1];
                oxgame->turn = oxgame->gamer_fd[0];
            }
            oxgame->left_step--;

            return true;
        }
    } else if(action == 3){
        if(oxgame->board[0][2] != ' '){
            goto invalid_action;
        } else {
            if(sockfd == oxgame->id){
                oxgame->board[0][2] = oxgame->gamer_label[0];
                oxgame->turn = oxgame->gamer_fd[1];
            }
            else{
                oxgame->board[0][2] = oxgame->gamer_label[1];
                oxgame->turn = oxgame->gamer_fd[0];
            }
            oxgame->left_step--;

            return true;
        }
    }else if(action == 4){
        if(oxgame->board[1][0] != ' '){
            goto invalid_action;
        } else {
            if(sockfd == oxgame->id){
                oxgame->board[1][0] = oxgame->gamer_label[0];
                oxgame->turn = oxgame->gamer_fd[1];
            }
            else{
                oxgame->board[1][0] = oxgame->gamer_label[1];
                oxgame->turn = oxgame->gamer_fd[0];
            }
            oxgame->left_step--;

            return true;
        }
    }else if(action == 5){
        if(oxgame->board[1][1] != ' '){
            goto invalid_action;
        } else {
            if(sockfd == oxgame->id){
                oxgame->board[1][1] = oxgame->gamer_label[0];
                oxgame->turn = oxgame->gamer_fd[1];
            }
            else {
                oxgame->board[1][1] = oxgame->gamer_label[1];
                oxgame->turn = oxgame->gamer_fd[0];
            }
            oxgame->left_step--;

            return true;
        }
    }else if(action == 6){
        if(oxgame->board[1][2] != ' '){
            goto invalid_action;
        } else {
            if(sockfd == oxgame->id){
                oxgame->board[1][2] = oxgame->gamer_label[0];
                oxgame->turn = oxgame->gamer_fd[1];
            }
            else {
                oxgame->board[1][2] = oxgame->gamer_label[1];
                oxgame->turn = oxgame->gamer_fd[0];
            }
            oxgame->left_step--;

            return true;
        }
    }else if(action == 7){
        if(oxgame->board[2][0] != ' '){
            goto invalid_action;
        } else {
            if(sockfd == oxgame->id){
                oxgame->board[2][0] = oxgame->gamer_label[0];
                oxgame->turn = oxgame->gamer_fd[1];
            }
            else {
                oxgame->board[2][0] = oxgame->gamer_label[1];
                oxgame->turn = oxgame->gamer_fd[0];
            }
            oxgame->left_step--;

            return true;
        }
    }else if(action == 8){
        if(oxgame->board[2][1] != ' '){
            goto invalid_action;
        } else {
            if(sockfd == oxgame->id){
                oxgame->board[2][1] = oxgame->gamer_label[0];
                oxgame->turn = oxgame->gamer_fd[1];
            }
            else{
                oxgame->board[2][1] = oxgame->gamer_label[1];
                oxgame->turn = oxgame->gamer_fd[0];
            }
            oxgame->left_step--;

            return true;
        }
    }else if(action == 9){
        if(oxgame->board[2][2] != ' '){
            goto invalid_action;
        } else {
            if(sockfd == oxgame->id){
                oxgame->board[2][2] = oxgame->gamer_label[0];
                oxgame->turn = oxgame->gamer_fd[1];
            }
            else {
                oxgame->board[2][2] = oxgame->gamer_label[1];
                oxgame->turn = oxgame->gamer_fd[0];
            }
            oxgame->left_step--;

            return true;
        }
    } else { // invalid action
        goto invalid_action;
    }
invalid_action:
    return false;
}

OXGame *get_game_by_sockfd(int sockfd){
    OXGame *oxgame;
    for(size_t i = 0; i < game_idx; ++i){
        oxgame = running_games[i];
        if(oxgame && (oxgame->gamer_fd[0] == sockfd || oxgame->gamer_fd[1] == sockfd))
            return oxgame;
    }
    return NULL;
}

OXNoti *get_noti_by_sockfd(int sockfd, int inv_fd){
    OXGamer *gamer;
    for(size_t i = 0; i < gamer_idx; ++i){
        gamer = login_gamers[i];
        if(gamer && gamer->sockfd == sockfd){
            gamer = login_gamers[i];
            break;
        }
    }

    OXNoti *noti;
    for(size_t i = 0; i < gamer->ox_noti_idx; ++i){
        noti = gamer->ox_noti[i];
        if(noti && noti->sockfd == inv_fd)
            return noti;
    }

    return NULL;
}

void draw_oxboard_to_watcher(int sockfd, OXGame *oxgame){
    char buf[1024] = {0};

    sprintf(buf, "\n%c|%c|%c\n------\n%c|%c|%c\n------\n%c|%c|%c\n",
        oxgame->board[0][0],oxgame->board[0][1],oxgame->board[0][2],
        oxgame->board[1][0],oxgame->board[1][1],oxgame->board[1][2],
        oxgame->board[2][0],oxgame->board[2][1],oxgame->board[2][2]);

    if(oxgame->turn == oxgame->gamer_fd[0])
        strcat(buf, "The turn is 'o'\n");
    else
        strcat(buf, "The turn is 'x'\n");

    send(sockfd, buf, strlen(buf), 0);
    return;
}
