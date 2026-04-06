#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "game.h"

#define HTTP_PORT 8081
#define HTTP_BUFFER 8192

// Referencias a las variables globales de server.c
extern GameData game;
extern pthread_mutex_t game_mutex;

// ── Utilidades HTTP ───────────────────────────────────

static void send_response(int fd, int code, const char* code_str,
                          const char* content_type, const char* body) {
    char header[256];
    int body_len = strlen(body);
    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, code_str, content_type, body_len);
    send(fd, header, strlen(header), 0);
    send(fd, body, body_len, 0);
}

// Extrae el body de una petición HTTP (lo que va después del \r\n\r\n)
static const char* get_body(const char* req) {
    const char* sep = strstr(req, "\r\n\r\n");
    if (sep) return sep + 4;
    return NULL;
}

// Extrae el valor de un campo en un body tipo "user=alice&pass=1234"
static void get_field(const char* body, const char* key, char* out, int out_size) {
    out[0] = '\0';
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char* pos = strstr(body, search);
    if (!pos) return;
    pos += strlen(search);
    int i = 0;
    while (pos[i] && pos[i] != '&' && pos[i] != '\r' && pos[i] != '\n' && i < out_size - 1) {
        out[i] = pos[i];
        i++;
    }
    out[i] = '\0';
}

// ── Páginas HTML ──────────────────────────────────────

// Página de login
static void build_login_page(char* buf, int buf_size, const char* error_msg) {
    snprintf(buf, buf_size,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>GCP - Login</title>"
        "<style>"
        "body{font-family:monospace;background:#1e1e1e;color:#ccc;"
        "display:flex;justify-content:center;align-items:center;height:100vh;margin:0}"
        ".box{background:#2a2a2a;padding:32px 40px;border:1px solid #444;min-width:280px}"
        "h2{color:#4af;margin-top:0}label{display:block;margin-bottom:4px;color:#aaa}"
        "input{width:100%%;padding:8px;background:#1e1e1e;border:1px solid #555;"
        "color:#ccc;font-family:monospace;box-sizing:border-box;margin-bottom:14px}"
        "button{width:100%%;padding:9px;background:#2255aa;color:white;border:none;"
        "font-family:monospace;font-size:14px;cursor:pointer}"
        "button:hover{background:#3366cc}"
        ".error{color:#f66;margin-bottom:12px}"
        "</style></head><body>"
        "<div class='box'>"
        "<h2>GCP - Acceso al juego</h2>"
        "%s"
        "<form method='POST' action='/login'>"
        "<label>Usuario</label>"
        "<input type='text' name='user' required autofocus>"
        "<label>Contraseña</label>"
        "<input type='password' name='pass' required>"
        "<button type='submit'>Entrar</button>"
        "</form>"
        "</div></body></html>",
        error_msg ? error_msg : "");
}

// Página de partidas activas (post-login)
static void build_partidas_page(char* buf, int buf_size,
                                const char* username, const char* role) {
    const char* state_str[] = {"WAITING", "RUNNING", "FINISHED"};
    const char* role_color = strcmp(role, "ATTACKER") == 0 ? "#f44" : "#44f";

    int offset = 0;
    offset += snprintf(buf + offset, buf_size - offset,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>GCP - Partidas</title>"
        "<style>"
        "body{font-family:monospace;background:#1e1e1e;color:#ccc;padding:20px;margin:0}"
        "h1{color:#4af}h2{color:#aaa}"
        "table{border-collapse:collapse;width:100%%;margin-bottom:24px}"
        "th{background:#333;color:#4af;padding:8px;text-align:left}"
        "td{padding:6px 8px;border-bottom:1px solid #333}"
        ".waiting{color:#fa0}.running{color:#4f4}.finished{color:#888}"
        ".tag{padding:2px 8px;border-radius:3px;font-size:12px}"
        ".btn{background:#2255aa;color:white;border:none;padding:6px 14px;"
        "font-family:monospace;cursor:pointer;border-radius:3px}"
        ".btn:hover{background:#3366cc}"
        ".info{background:#222;border:1px solid #444;padding:12px;margin-bottom:20px}"
        "</style></head><body>"
        "<h1>GCP - Partidas activas</h1>"
        "<div class='info'>"
        "Usuario: <b>%s</b> &nbsp;|&nbsp; "
        "Rol: <b style='color:%s'>%s</b>"
        "</div>",
        username, role_color, role);

    pthread_mutex_lock(&game_mutex);

    if (game.room_count == 0) {
        offset += snprintf(buf + offset, buf_size - offset,
            "<p>No hay partidas creadas. Abre el cliente y conéctate para crear una.</p>");
    } else {
        offset += snprintf(buf + offset, buf_size - offset,
            "<table>"
            "<tr><th>Sala</th><th>Estado</th><th>Jugadores</th><th>Acción</th></tr>");

        for (int i = 0; i < game.room_count; i++) {
            Room* r = &game.rooms[i];
            const char* st = state_str[r->state];
            const char* st_class = r->state == WAITING  ? "waiting"
                                 : r->state == RUNNING  ? "running"
                                 : "finished";

            offset += snprintf(buf + offset, buf_size - offset,
                "<tr>"
                "<td>#%d</td>"
                "<td class='%s'>%s</td>"
                "<td>%d jugador(es)</td>"
                "<td><button class='btn' onclick=\"unirse(%d, '%s')\">Unirse</button></td>"
                "</tr>",
                r->id, st_class, st, r->player_count, r->id, username);
        }

        offset += snprintf(buf + offset, buf_size - offset, "</table>");
    }

    pthread_mutex_unlock(&game_mutex);

    offset += snprintf(buf + offset, buf_size - offset,
        "<p style='color:#888'>Para unirte a una sala, abre el cliente "
        "(Java o Python) e ingresa el ID de sala mostrado arriba.</p>"
        "<button class='btn' onclick='location.reload()'>Actualizar</button>"
        "<script>"
        "function unirse(id, user) {"
        "  alert('Abre el cliente GCP e ingresa:\\nSala ID: ' + id + '\\nUsuario: ' + user);"
        "}"
        "</script>"
        "</body></html>");
}

// ── Manejo de rutas ───────────────────────────────────

static void handle_http_client(int client_fd) {
    char req[HTTP_BUFFER];
    int bytes = recv(client_fd, req, sizeof(req) - 1, 0);
    if (bytes <= 0) { close(client_fd); return; }
    req[bytes] = '\0';

    // GET / → mostrar login
    if (strncmp(req, "GET / ", 6) == 0 ||
        strncmp(req, "GET /index.html ", 16) == 0) {

        char body[HTTP_BUFFER];
        build_login_page(body, sizeof(body), NULL);
        send_response(client_fd, 200, "OK", "text/html; charset=UTF-8", body);

    // POST /login → validar credenciales
    } else if (strncmp(req, "POST /login ", 12) == 0) {

        const char* body_ptr = get_body(req);
        char username[64] = {0};
        char password[64] = {0};

        if (body_ptr) {
            get_field(body_ptr, "user", username, sizeof(username));
            get_field(body_ptr, "pass", password, sizeof(password));
        }

        // Consultar auth server verificando usuario y contraseña
        char role[16] = {0};
        int found = query_auth_server(username, password, role);

        if (!found || username[0] == '\0') {
            // Usuario no encontrado → volver al login con error
            char page[HTTP_BUFFER];
            build_login_page(page, sizeof(page),
                "<p class='error'>Usuario o contraseña incorrectos.</p>");
            send_response(client_fd, 200, "OK", "text/html; charset=UTF-8", page);
        } else {
            // Login OK → mostrar partidas
            char page[HTTP_BUFFER * 2];
            build_partidas_page(page, sizeof(page), username, role);
            send_response(client_fd, 200, "OK", "text/html; charset=UTF-8", page);
        }

    // Cualquier otra ruta → 404
    } else {
        send_response(client_fd, 404, "Not Found", "text/plain", "404 Not Found");
    }

    close(client_fd);
}

// ── Hilo del servidor HTTP ────────────────────────────

static void* http_thread(void* arg) {
    (void)arg;

    int server_fd;
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("[HTTP] Error creando socket"); return NULL; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(HTTP_PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[HTTP] Error en bind"); close(server_fd); return NULL;
    }
    if (listen(server_fd, 10) < 0) {
        perror("[HTTP] Error en listen"); close(server_fd); return NULL;
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
