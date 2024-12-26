// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>
#include "game.h"

#define PORT 8080
#define MAX_SESSIONS 100

typedef struct {
    char session_id[17]; // 16 + '\0'
    Game game;
    int player1_connected;
    int player2_connected;
    int current_turn; // 1 или 2
} Session;

Session *sessions;

// Старая функция генерации 16-символьного ID
static void generate_session_id(char *buffer, size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t charset_size = sizeof(charset)-1;
    if(length < 17) {
        if(length>0) buffer[0]='\0';
        return;
    }
    for(size_t i=0; i<16; i++){
        buffer[i] = charset[rand() % charset_size];
    }
    buffer[16] = '\0';
}

// Проверяет, есть ли уже такая session_id
int find_session(const char *session_id) {
    for(int i = 0; i < MAX_SESSIONS; i++) {
        if(strcmp(sessions[i].session_id, session_id) == 0) {
            return i;
        }
    }
    return -1;
}

// Генерация ID с проверкой коллизий
void generate_unique_session_id(char *buffer, size_t length) {
    if(length<17) {
        if(length>0) buffer[0]='\0';
        return;
    }
    for(int attempt=0; attempt<100; attempt++){
        generate_session_id(buffer, length);
        if(find_session(buffer) == -1) {
            // уникальный
            return;
        }
    }
    // если всё плохо — вернём пустую строку
    buffer[0] = '\0';
}

// Упрощённая отправка ответов (без Cookie)
void send_response(int sock, const char *status,
                   const char *content_type,
                   const char *body, size_t length) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, content_type, length
    );
    send(sock, header, header_len, 0);
    send(sock, body, length, 0);
}

const char* get_content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if(!ext) return "application/octet-stream";
    if(strcmp(ext, ".html") == 0) return "text/html";
    if(strcmp(ext, ".css") == 0) return "text/css";
    if(strcmp(ext, ".js") == 0) return "application/javascript";
    return "application/octet-stream";
}

// Порционное чтение и отправка статического файла
void handle_static_file(int sock, const char *path) {
    char filepath[1024];
    if(strcmp(path,"/")==0){
        snprintf(filepath,sizeof(filepath),"www/index.html");
    } else {
        if(path[0]=='/') {
            snprintf(filepath,sizeof(filepath),"www%s",path);
        } else {
            snprintf(filepath,sizeof(filepath),"www/%s",path);
        }
    }

    FILE *file = fopen(filepath,"rb");
    if(!file){
        const char *msg="404 Not Found";
        send_response(sock,"404 Not Found","text/plain",
                      msg,strlen(msg));
        printf("File not found: %s\n", filepath);
        return;
    }

    fseek(file,0,SEEK_END);
    long filesize=ftell(file);
    rewind(file);

    if(filesize<0){
        fclose(file);
        const char *msg="500 Internal Server Error";
        send_response(sock,"500 Internal Server Error","text/plain",
                      msg,strlen(msg));
        return;
    }

    const char *ctype=get_content_type(filepath);

    // отправка заголовков
    {
        char header[512];
        int head_len=snprintf(header,sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n",
            ctype,filesize
        );
        send(sock,header,head_len,0);
    }

    // отправляем сам файл
    char buf[8192];
    size_t rd=0;
    while((rd=fread(buf,1,sizeof(buf),file))>0){
        send(sock,buf,rd,0);
    }

    fclose(file);
    printf("Served file: %s (size=%ld)\n", filepath, filesize);
}

// Создание новой сессии (player1)
void handle_create_session(int sock) {
    int free_index=-1;
    for(int i=0;i<MAX_SESSIONS;i++){
        if(sessions[i].session_id[0]=='\0'){
            free_index=i;
            break;
        }
    }
    if(free_index==-1){
        const char *msg="503 Service Unavailable: No free sessions";
        send_response(sock,"503 Service Unavailable","text/plain",
                      msg,strlen(msg));
        return;
    }

    char sid[17];
    generate_unique_session_id(sid,sizeof(sid));
    if(sid[0]=='\0'){
        const char *msg="503 Service Unavailable: can't create unique ID";
        send_response(sock,"503 Service Unavailable","text/plain",
                      msg,strlen(msg));
        return;
    }

    strcpy(sessions[free_index].session_id, sid);
    init_game(&sessions[free_index].game);
    sessions[free_index].player1_connected=1;
    sessions[free_index].player2_connected=0;
    sessions[free_index].current_turn=1;

    char json[200];
    int written=snprintf(json,sizeof(json),
        "{\"session_id\": \"%s\", \"player\": 1}", sid);
    send_response(sock,"200 OK","application/json", json, written);

    printf("Created session %s (index=%d) as player1\n", sid, free_index);
}

// Присоединение (player2)
void handle_join_session(int sock, const char *sid){
    int idx=find_session(sid);
    if(idx==-1){
        const char *msg="404 Not Found: no such session";
        send_response(sock,"404 Not Found","text/plain",msg,strlen(msg));
        return;
    }
    if(sessions[idx].player2_connected){
        const char *msg="403 Forbidden: session is full";
        send_response(sock,"403 Forbidden","text/plain",msg,strlen(msg));
        return;
    }
    sessions[idx].player2_connected=1;

    char json[200];
    int written=snprintf(json,sizeof(json),
        "{\"message\": \"Joined session %s\", \"player\": 2}", sid);
    send_response(sock,"200 OK","application/json", json, written);

    printf("Joined session %s (index=%d) as player2\n", sid, idx);
}

// Сброс игры
void handle_reset_game(int sock, const char *sid){
    int idx=find_session(sid);
    if(idx==-1){
        const char *msg="404 Not Found: no such session";
        send_response(sock,"404 Not Found","text/plain", msg, strlen(msg));
        return;
    }
    init_game(&sessions[idx].game);
    sessions[idx].current_turn=1;

    char json[200];
    int written=snprintf(json,sizeof(json),
        "{\"message\":\"Game reset in session %s\"}",sid);
    send_response(sock,"200 OK","application/json", json, written);

    printf("Game reset in session %s\n", sid);
}

// Сделать ход
void handle_make_move(int sock, const char *sid, int col, int player){
    int idx=find_session(sid);
    if(idx==-1){
        const char *msg="404 Not Found: no such session";
        send_response(sock,"404 Not Found","text/plain",msg,strlen(msg));
        return;
    }
    if(player==1 && !sessions[idx].player1_connected){
        const char *msg="403 Forbidden: player1 not in session";
        send_response(sock,"403 Forbidden","text/plain",msg,strlen(msg));
        return;
    }
    if(player==2 && !sessions[idx].player2_connected){
        const char *msg="403 Forbidden: player2 not in session";
        send_response(sock,"403 Forbidden","text/plain",msg,strlen(msg));
        return;
    }
    if(sessions[idx].current_turn!=player){
        const char *msg="403 Forbidden: not your turn";
        send_response(sock,"403 Forbidden","text/plain",msg,strlen(msg));
        return;
    }

    Cell c=(player==1)? USER: SERVER;
    if(drop_piece(&sessions[idx].game,col,c)==-1){
        const char *msg="400 Bad Request: invalid move";
        send_response(sock,"400 Bad Request","text/plain",msg,strlen(msg));
        return;
    }

    sessions[idx].game.status=check_winner(&sessions[idx].game);
    if(sessions[idx].game.status==ONGOING){
        sessions[idx].current_turn=(player==1)?2:1;
    }

    char board_json[8192];
    board_to_json(&sessions[idx].game, board_json, sizeof(board_json));

    char response[8192];
    int written=snprintf(response,sizeof(response),
        "{\"board\": %s, \"status\": \"%s\", \"current_turn\": %d}",
        board_json,
        (sessions[idx].game.status==WIN_USER)? "win_user":
        (sessions[idx].game.status==WIN_SERVER)? "win_server":
        (sessions[idx].game.status==DRAW)? "draw": "ongoing",
        sessions[idx].current_turn);

    if(written>=(int)sizeof(response)){
        const char *msg="500 Internal Server Error: JSON too large";
        send_response(sock,"500 Internal Server Error","text/plain",
                      msg,strlen(msg));
        return;
    }

    send_response(sock,"200 OK","application/json",response,strlen(response));
    printf("Session %s: player %d => column %d\n", sid,player,col);
}

// Получение состояния игры
void handle_get_state(int sock, const char *sid){
    int idx=find_session(sid);
    if(idx==-1){
        const char *msg="404 Not Found: no such session";
        send_response(sock,"404 Not Found","text/plain",msg,strlen(msg));
        return;
    }

    char board_json[8192];
    board_to_json(&sessions[idx].game, board_json, sizeof(board_json));

    char response[8192];
    int written=snprintf(response,sizeof(response),
        "{\"board\": %s, \"status\": \"%s\", \"current_turn\": %d}",
        board_json,
        (sessions[idx].game.status==WIN_USER)? "win_user":
        (sessions[idx].game.status==WIN_SERVER)? "win_server":
        (sessions[idx].game.status==DRAW)? "draw": "ongoing",
        sessions[idx].current_turn);

    if(written>=(int)sizeof(response)){
        const char *msg="500 Internal Server Error: JSON too large";
        send_response(sock,"500 Internal Server Error","text/plain",
                      msg,strlen(msg));
        return;
    }

    send_response(sock,"200 OK","application/json",response,strlen(response));
}

// Обработка HTTP-запроса
void handle_client_request(int sock){
    char buffer[8192];
    int recvd=recv(sock,buffer,sizeof(buffer)-1,0);
    if(recvd<0){
        perror("recv");
        close(sock);
        exit(EXIT_FAILURE);
    }
    buffer[recvd]='\0';

    char method[8], path[1024], prot[16];
    sscanf(buffer,"%s %s %s",method,path,prot);

    if(strcmp(method,"GET")==0){
        if(strncmp(path,"/get_state",10)==0){
            char *q=strstr(path,"?");
            char sid[17]="";
            if(q){
                sscanf(q,"?session_id=%16s",sid);
                char *amp=strchr(sid,'&');
                if(amp) *amp='\0';
            }
            if(strlen(sid)){
                handle_get_state(sock,sid);
            } else {
                const char *msg="400 Bad Request: no sid";
                send_response(sock,"400 Bad Request","text/plain",
                              msg,strlen(msg));
            }
        } else {
            handle_static_file(sock,path);
        }
    }
    else if(strcmp(method,"POST")==0){
        char *body=strstr(buffer,"\r\n\r\n");
        if(body) body+=4; else body="";

        if(strcmp(path,"/create_session")==0){
            // Всегда генерируем новую сессию,
            // игнорируем любые куки
            handle_create_session(sock);
        }
        else if(strcmp(path,"/join_session")==0){
            char sid[17]="";
            sscanf(body,"session_id=%16s",sid);
            if(strlen(sid)){
                handle_join_session(sock,sid);
            } else {
                const char *msg="400 Bad Request: no sid";
                send_response(sock,"400 Bad Request","text/plain",
                              msg,strlen(msg));
            }
        }
        else if(strcmp(path,"/reset_game")==0){
            char sid[17]="";
            sscanf(body,"session_id=%16s",sid);
            if(strlen(sid)){
                handle_reset_game(sock,sid);
            } else {
                const char *msg="400 Bad Request: no sid";
                send_response(sock,"400 Bad Request","text/plain",
                              msg,strlen(msg));
            }
        }
        else if(strcmp(path,"/make_move")==0){
            char sid[17]="";
            int col=-1, player=-1;
            sscanf(body,"session_id=%16[^&]&column=%d&player=%d", sid, &col, &player);
            if(strlen(sid)&&col>=0&&col<COLUMNS&&(player==1||player==2)){
                handle_make_move(sock,sid,col,player);
            } else {
                const char *msg="400 Bad Request: invalid params";
                send_response(sock,"400 Bad Request","text/plain",
                              msg,strlen(msg));
            }
        }
        else {
            const char *msg="404 Not Found";
            send_response(sock,"404 Not Found","text/plain",msg,strlen(msg));
        }
    }
    else {
        const char *msg="405 Method Not Allowed";
        send_response(sock,"405 Method Not Allowed","text/plain",
                      msg,strlen(msg));
    }

    close(sock);
}

// SIGINT
void sigint_handler(int sig){
    printf("\nShutting down...\n");
    munmap(sessions,sizeof(Session)*MAX_SESSIONS);
    exit(0);
}

int main(){
    srand(time(NULL)); // Один раз seed
    signal(SIGINT,sigint_handler);

    sessions=mmap(NULL,sizeof(Session)*MAX_SESSIONS,
                  PROT_READ|PROT_WRITE,
                  MAP_SHARED|MAP_ANONYMOUS,-1,0);
    if(sessions==MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    for(int i=0;i<MAX_SESSIONS;i++){
        sessions[i].session_id[0]='\0';
        init_game(&sessions[i].game);
        sessions[i].player1_connected=0;
        sessions[i].player2_connected=0;
        sessions[i].current_turn=1;
    }

    int server_fd;
    struct sockaddr_in addr;
    socklen_t addrlen=sizeof(addr);

    if((server_fd=socket(AF_INET,SOCK_STREAM,0))==0){
        perror("socket failed");
        munmap(sessions,sizeof(Session)*MAX_SESSIONS);
        exit(EXIT_FAILURE);
    }
    int opt=1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons(PORT);

    if(bind(server_fd,(struct sockaddr*)&addr,sizeof(addr))<0){
        perror("bind");
        close(server_fd);
        munmap(sessions,sizeof(Session)*MAX_SESSIONS);
        exit(EXIT_FAILURE);
    }

    if(listen(server_fd,10)<0){
        perror("listen");
        close(server_fd);
        munmap(sessions,sizeof(Session)*MAX_SESSIONS);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n",PORT);

    while(1){
        int new_sock=accept(server_fd,(struct sockaddr*)&addr,&addrlen);
        if(new_sock<0){
            perror("accept");
            continue;
        }
        pid_t pid=fork();
        if(pid<0){
            perror("fork");
            close(new_sock);
            continue;
        }
        if(pid==0){
            close(server_fd);
            handle_client_request(new_sock);
            munmap(sessions,sizeof(Session)*MAX_SESSIONS);
            exit(0);
        } else {
            close(new_sock);
            while(waitpid(-1,NULL,WNOHANG)>0);
        }
    }

    close(server_fd);
    munmap(sessions,sizeof(Session)*MAX_SESSIONS);
    return 0;
}
