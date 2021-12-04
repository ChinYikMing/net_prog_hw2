typedef struct ox_noti {
    char name[16];
    int sockfd;
} OXNoti;

typedef struct ox_game {
    int id;              // socket fd of the initiator
    char board[3][3];
    int turn;            // always start from initiator
    int left_step;
    int status;
    int watchers[1024];
    int watchers_idx;
    int gamer_fd[2];
    char gamer_label[2]; // initiator always is 'o', peer always is 'x'
} OXGame;

typedef struct ox_gamer {
    char name[16];
    int sockfd;
    int status;
    OXNoti *ox_noti[64];
    int ox_noti_idx;
} OXGamer;

OXGamer *create_oxgamer(char *name, int sockfd);
OXGame *create_oxgame(int sockfd1, int sockfd2);
OXNoti *create_oxnoti(char *initiator_name, int initiator_fd);
void draw_oxboard_to_watcher(int sockfd, OXGame *oxgame);
void draw_oxboard(int sockfd, int msg_cnt, ...); // one of the fd of the gamer
_Bool update_oxboard(int sockfd, int action); // one of the fd of the gamer
_Bool is_win(OXGame *ox_game);
_Bool is_draw(OXGame *ox_game);
char *get_name_by_sockfd(int sockfd);
OXGame *get_game_by_sockfd(int sockfd);
OXNoti *get_noti_by_sockfd(int sockfd, int inv_fd);
void add_gamer(char *name, int sockfd);
void del_gamer(int sockfd);
void add_invitation(int inv_fd, int sockfd);
void del_invitation(int inv_fd, int sockfd);
void add_game(int inv_fd, int accept_fd);
void del_game(OXGame *game);
_Bool req_parser(char *cmd, int initiator_fd);