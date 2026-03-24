#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "game.h"

#define HTTP_PORT 8081
#define HTTP_BUFFER 4096

// Referencias a las variables globales de server.c
extern GameData game;
extern pthread_mutex_t game_mutex;

// ── Construcción del HTML ─────────────────────────────

static void build_html(char* buf, int buf_size) {
    const char* state_str[] = {"WAITING", "RUNNING", "FINISHED"};
    const char* role_str[]  = {"ATTACKER", "DEFENDER"};

    int offset = 0;

    offset += snprintf(buf + offset, buf_size - offset,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<title>GCP - Partidas activas</title>"
        "<style>"
        "body{font-family:monospace;background:#1e1e1e;color:#ccc;padding:20px}"
        "h1{color:#4af;display:inline-block;margin-right:16px}"
        "table{border-collapse:collapse;width:100%%;margin-bottom:30px}"
        "th{background:#333;color:#4af;padding:8px;text-align:left}"
        "td{padding:6px 8px;border-bottom:1px solid #333}"
        "tr:hover td{background:#2a2a2a}"
        ".waiting{color:#fa0}.running{color:#4f4}.finished{color:#888}"
        ".attacker{color:#f44}.defender{color:#44f}"
        "h2{color:#aaa;margin-top:30px}"
        ".btn-refresh{background:#2255aa;color:white;border:none;padding:8px 18px;"
        "font-family:monospace;font-size:14px;cursor:pointer;border-radius:4px}"
        ".btn-refresh:hover{background:#3366cc}"
        ".header{display:flex;align-items:center;margin-bottom:10px}"
        "</style></head><body>"
        "<div class='header'>"
        "<h1>GCP - Game Communication Protocol</h1>"
        "<button class='btn-refresh' onclick='location.reload()'>Recargar</button>"
        "</div>"
        "<p>Partidas activas</p>"
    );

    pthread_mutex_lock(&game_mutex);

    if (game.room_count == 0) {
        offset += snprintf(buf + offset, buf_size - offset,
            "<p>No hay partidas creadas aún.</p>");
    }

    for (int i = 0; i < game.room_count; i++) {
        Room* r = &game.rooms[i];
        const char* st = state_str[r->state];
        const char* st_class = r->state == WAITING ? "waiting"
                             : r->state == RUNNING  ? "running"
                             : "finished";

        offset += snprintf(buf + offset, buf_size - offset,
            "<h2>Sala #%d &mdash; <span class='%s'>%s</span> &mdash; %d jugador(es)</h2>",
            r->id, st_class, st, r->player_count);

        // Tabla de jugadores
        offset += snprintf(buf + offset, buf_size - offset,
            "<table>"
            "<tr><th>Usuario</th><th>Rol</th><th>Posicion</th></tr>");

        for (int j = 0; j < r->player_count; j++) {
            Player* p = r->players[j];
            if (!p || !p->active) continue;
            const char* rol = role_str[p->role];
            const char* rol_class = p->role == ATTACKER ? "attacker" : "defender";
            offset += snprintf(buf + offset, buf_size - offset,
                "<tr><td>%s</td><td class='%s'>%s</td><td>(%d, %d)</td></tr>",
                p->username, rol_class, rol, p->x, p->y);
        }

        // Tabla de recursos
        offset += snprintf(buf + offset, buf_size - offset,
            "</table>"
            "<table>"
            "<tr><th>Recurso ID</th><th>Posicion</th><th>Estado</th></tr>");

        for (int k = 0; k < MAX_RESOURCES; k++) {
            Resource* res = &r->resources[k];
            const char* atk = res->under_attack ? "<span class='attacker'>BAJO ATAQUE</span>"
                                                : "<span class='running'>OK</span>";
            offset += snprintf(buf + offset, buf_size - offset,
                "<tr><td>%d</td><td>(%d, %d)</td><td>%s</td></tr>",
                res->id, res->x, res->y, atk);
        }

        offset += snprintf(buf + offset, buf_size - offset, "</table>");
    }

    pthread_mutex_unlock(&game_mutex);

    offset += snprintf(buf + offset, buf_size - offset,
        "</body></html>");
}

// ── Manejo de una conexión HTTP ───────────────────────

static void handle_http_client(int client_fd) {
    char req[HTTP_BUFFER];
    int bytes = recv(client_fd, req, sizeof(req) - 1, 0);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }
    req[bytes] = '\0';

    // Solo atendemos GET /
    int is_root = (strncmp(req, "GET / ", 6) == 0 ||
                   strncmp(req, "GET /index.html ", 16) == 0);

    if (!is_root) {
        const char* not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "404 Not Found";
        send(client_fd, not_found, strlen(not_found), 0);
        close(client_fd);
        return;
    }

    char body[HTTP_BUFFER * 3];
    build_html(body, sizeof(body));

    char header[256];
    int body_len = strlen(body);
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        body_len);

    send(client_fd, header, strlen(header), 0);
    send(client_fd, body,   body_len,       0);
    close(client_fd);
}

// ── Hilo del servidor HTTP ────────────────────────────

static void* http_thread(void* arg) {
    (void)arg;

    int server_fd;
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[HTTP] Error creando socket");
        return NULL;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(HTTP_PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[HTTP] Error en bind");
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 10) < 0) {
        perror("[HTTP] Error en listen");
        close(server_fd);
        return NULL;
    }

    printf("Servidor HTTP corriendo en http://localhost:%d\n", HTTP_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) continue;
        handle_http_client(client_fd);
    }

    close(server_fd);
    return NULL;
}

// ── Punto de entrada público ──────────────────────────

void start_http_server(void) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, http_thread, NULL) == 0) {
        pthread_detach(tid);
    } else {
        perror("[HTTP] Error creando hilo");
    }
}
