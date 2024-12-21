// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include "game.h"

#define PORT 8080
#define BUFFER_SIZE 4096

Game game; // Глобальная переменная для состояния игры

void handle_sigchld(int sig) {
    // Обработка завершения дочерних процессов для предотвращения зомби-процессов
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void send_response(int client_fd, const char *content, const char *content_type) {
    char response[BUFFER_SIZE];
    int content_length = strlen(content);
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             content_type, content_length, content);
    send(client_fd, response, strlen(response), 0);
}

void handle_request(int client_fd) {
    char buffer[BUFFER_SIZE];
    int received = recv(client_fd, buffer, sizeof(buffer)-1, 0);
    if(received < 0){
        perror("recv");
        close(client_fd);
        exit(1);
    }
    buffer[received] = '\0';

    // Простая обработка HTTP GET и POST запросов
    if (strncmp(buffer, "GET / ", 6) == 0 || strncmp(buffer, "GET /index.html", 16) == 0) {
        // Обслуживание index.html
        FILE *file = fopen("www/index.html", "r");
        if (file == NULL) {
            const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
            send(client_fd, not_found, strlen(not_found), 0);
            close(client_fd);
            exit(0);
        }
        char *html = malloc(BUFFER_SIZE);
        fread(html, 1, BUFFER_SIZE, file);
        fclose(file);
        send_response(client_fd, html, "text/html");
        free(html);
    }
    else if (strncmp(buffer, "GET /style.css", 13) == 0) {
        // Обслуживание style.css
        FILE *file = fopen("www/style.css", "r");
        if (file == NULL) {
            const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
            send(client_fd, not_found, strlen(not_found), 0);
            close(client_fd);
            exit(0);
        }
        char *css = malloc(BUFFER_SIZE);
        fread(css, 1, BUFFER_SIZE, file);
        fclose(file);
        send_response(client_fd, css, "text/css");
        free(css);
    }
    else if (strncmp(buffer, "GET /script.js", 14) == 0) {
        // Обслуживание script.js
        FILE *file = fopen("www/script.js", "r");
        if (file == NULL) {
            const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
            send(client_fd, not_found, strlen(not_found), 0);
            close(client_fd);
            exit(0);
        }
        char *js = malloc(BUFFER_SIZE);
        fread(js, 1, BUFFER_SIZE, file);
        fclose(file);
        send_response(client_fd, js, "application/javascript");
        free(js);
    }
    else if (strncmp(buffer, "POST /move", 10) == 0) {
        // Обработка хода игрока
        // Предполагается, что тело запроса содержит номер столбца
        char *body = strstr(buffer, "\r\n\r\n");
        if (body != NULL) {
            body += 4; // Пропустить "\r\n\r\n"
            int col = atoi(body) - 1; // Преобразовать в индекс (0-6)
            if(make_move(&game, col)) {
                // Успешный ход
                char response_body[BUFFER_SIZE];
                snprintf(response_body, sizeof(response_body), "{\"board\": [");
                for(int i = 0; i < ROWS; i++) {
                    snprintf(response_body + strlen(response_body), sizeof(response_body) - strlen(response_body), "[");
                    for(int j = 0; j < COLS; j++) {
                        snprintf(response_body + strlen(response_body), sizeof(response_body) - strlen(response_body), "\"%c\"", game.board[i][j]);
                        if(j < COLS -1) strcat(response_body, ",");
                    }
                    snprintf(response_body + strlen(response_body), sizeof(response_body) - strlen(response_body), "]");
                    if(i < ROWS -1) strcat(response_body, ",");
                }
                strcat(response_body, "], \"current_player\": ");
                char player[2];
                snprintf(player, sizeof(player), "%d", game.current_player);
                strcat(response_body, player);
                strcat(response_body, "}");
                send_response(client_fd, response_body, "application/json");
            } else {
                // Ошибка хода
                const char *error = "{\"error\": \"Invalid move\"}";
                send_response(client_fd, error, "application/json");
            }
        }
    }
    else if (strncmp(buffer, "GET /state", 10) == 0) {
        // Отправка текущего состояния игры
        char response_body[BUFFER_SIZE];
        snprintf(response_body, sizeof(response_body), "{\"board\": [");
        for(int i = 0; i < ROWS; i++) {
            snprintf(response_body + strlen(response_body), sizeof(response_body) - strlen(response_body), "[");
            for(int j = 0; j < COLS; j++) {
                snprintf(response_body + strlen(response_body), sizeof(response_body) - strlen(response_body), "\"%c\"", game.board[i][j]);
                if(j < COLS -1) strcat(response_body, ",");
            }
            snprintf(response_body + strlen(response_body), sizeof(response_body) - strlen(response_body), "]");
            if(i < ROWS -1) strcat(response_body, ",");
        }
        strcat(response_body, "], \"current_player\": ");
        char player[2];
        snprintf(player, sizeof(player), "%d", game.current_player);
        strcat(response_body, player);
        strcat(response_body, "}");
        send_response(client_fd, response_body, "application/json");
    }
    else {
        // Неизвестный запрос
        const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
        send(client_fd, not_found, strlen(not_found), 0);
    }

    close(client_fd);
}

int main() {
    // Инициализация игры
    init_game(&game);

    // Обработка сигнала SIGCHLD для предотвращения зомби-процессов
    signal(SIGCHLD, handle_sigchld);

    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Настройка сокета
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Любой IP-адрес
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Сервер запущен на порту %d\n", PORT);

    while(1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("accept");
            continue;
        }

        // Обработка запроса в дочернем процессе
        if (fork() == 0) {
            // Дочерний процесс
            close(server_fd);
            handle_request(new_socket);
            exit(0);
        }
        // Родительский процесс закрывает дескриптор клиента
        close(new_socket);
    }

    close(server_fd);
    return 0;
}
