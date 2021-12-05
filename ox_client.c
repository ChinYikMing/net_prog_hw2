#define _GNU_SOURCE
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#ifndef max
#define max(a, b) ({ \
    __auto_type max_a = (a); \
    __auto_type max_b = (b); \
    max_a > max_b ? max_a : max_b; \
})
#endif

#define BUF_SIZE 1024
#define PORT 8000

static void usage(){
    printf("---------------------------------\n");
    printf("lsgamers\n");
    printf("    -to show all login gamers\n");
    printf("lsgames\n");
    printf("    -to show all running games\n");
    printf("lsnoti\n");
    printf("    -to show all notification\n");
    printf("exitgame\n");
    printf("    -exit the game\n");
    printf("leavegame\n");
    printf("    -leave the game\n");
    printf("logout\n");
    printf("    -logout current gamer\n");
    printf("watchgame <game ID>\n");
    printf("    -to watch a specific game\n");
    printf("invgamer <gamer ID>\n");
    printf("    -invite a specific gamer\n");
    printf("accept <gamer ID>\n");
    printf("    -accept a specific gamer\n");
    printf("reject <gamer ID>\n");
    printf("    -reject a specific gamer\n");
    printf("help\n");
    printf("    -to show these usage\n");
    printf("---------------------------------\n");
    return;
}

static _Bool login(int sockfd){
    char buf[BUF_SIZE] = {0};
    char login_req[128] = {0};
    char username[32] = {0};
    char pwd[32] = {0};
    char rec[32] = {0};

    printf("Enter username: ");
    fgets(username, 32, stdin);
    username[strcspn(username, "\r\n")] = 0;

    printf("Enter password: ");
    fgets(pwd, 32, stdin);
    pwd[strcspn(pwd, "\r\n")] = 0;
    printf("\n");

    strcpy(rec, username);
    strcat(rec, ":");
    strcat(rec, pwd);

    strcpy(login_req, "login");
    strcat(login_req, rec);
    strcat(login_req, " ");

    send(sockfd, login_req, strlen(login_req), 0);

    recv(sockfd, buf, BUF_SIZE, 0);
    if(strstr(buf, "failure")){
        printf("invalid username or password, please try again\n");
        return false;
    } else if(strstr(buf, "before")){
        printf("%s", buf);
        return false;
    }

    return true;
}

static _Bool cmd_parser(char *cmd){
    if(strstr(cmd, "lsgamers") || strstr(cmd, "lsgames") || strstr(cmd, "lsnoti") ||
        strstr(cmd, "exitgame") || strstr(cmd, "watchgame") || strstr(cmd, "invgamer") ||
        strstr(cmd, "accept") || strstr(cmd, "reject") || strstr(cmd, "play") ||
        strstr(cmd, "logout") || strstr(cmd, "leavegame"))
        return true;

    return false;
}

int main(int argc, char *argv[]){
    int sfd;
connect:
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sfd == -1){
        perror("socket");
        exit(1);
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    struct in_addr addr;
    if(inet_pton(AF_INET, "127.0.0.1", &addr) == -1){
        perror("inet_pton");
        exit(1);
    }
    sin.sin_addr = addr;

    if(connect(sfd, (struct sockaddr *) &sin, sizeof(sin)) < 0){
        perror("connect");
        exit(1);
    }

    char cmd[64];
    char buf[BUF_SIZE] = {0};
    size_t recv_size;
    int login_trial = 0;

again:
    if(!login(sfd)){
        login_trial++;
        if(login_trial == 3)
            goto too_many;
        else
            goto again;
    }

    printf("enter your cmd: (enter 'help' to see all valid cmd)\n");
    int maxfd;
    fd_set rset;
    
    FD_ZERO(&rset);
    FD_SET(fileno(stdin), &rset);
    FD_SET(sfd, &rset);
    maxfd = max(fileno(stdin), sfd) + 1;

    for(;;){
        FD_SET(fileno(stdin), &rset);
        FD_SET(sfd, &rset);

        memset(buf, 0, sizeof(char[BUF_SIZE]));

        select(maxfd, &rset, NULL, NULL, NULL);

        if(FD_ISSET(fileno(stdin), &rset)){
            fgets(cmd, BUF_SIZE, stdin);
            if(strstr(cmd, "help")){
                usage();
                continue;
            }

            if(cmd_parser(cmd)){
                send(sfd, cmd, strlen(cmd), 0);
            } else {
                printf("invalid cmd (enter 'help' to see all valid cmd)\n");
            }
        }

        if(FD_ISSET(sfd, &rset)){
            recv(sfd, buf, BUF_SIZE, 0);
            if(strstr(buf, "exitg"))
                goto end;
            else if(strstr(buf, "logout")){
                close(sfd);
                goto connect;
            }
            printf("%s", buf);
        }
    }

too_many:
    printf("Try too many times! Please try again!\n");
end:
    exit(0);
}