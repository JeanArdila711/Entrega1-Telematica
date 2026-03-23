#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "game.h"

// ── Variables globales ───────────────────────────────
GameData game;
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE* log_file = NULL;

// ── Logging ──────────────────────────────────────────
void log_message(const char* client_ip, int client_port, const char* type, const char* message) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    printf("[%s] [%s:%d] %s: %s", timestamp, client_ip, client_port, type, message);

    if (log_file) {
        fprintf(log_file, "[%s] [%s:%d] %s: %s", timestamp, client_ip, client_port, type, message);
        fflush(log_file);
    }
}

// ── Parsing de comandos ──────────────────────────────
void handle_client_message(Player* player, Room** room, const char* message,
                            char* response, const char* client_ip, int client_port) {

    log_message(client_ip, client_port, "REQUEST", message);

    char cmd[32] = {0};
    char param1[64] = {0};
    char param2[64] = {0};

    sscanf(message, "%s %s %s", cmd, param1, param2);

    // JOIN <sala_id> <username>
    if (strcmp(cmd, "JOIN") == 0) {
        int sala_id = atoi(param1);
        strncpy(player->username, param2, MAX_USERNAME - 1);
        player->active = 1;

        pthread_mutex_lock(&game_mutex);

        if (sala_id == 0) {
            // Buscar sala en espera antes de crear una nueva
            *room = NULL;
            for (int i = 0; i < game.room_count; i++) {
                if (game.rooms[i].state == WAITING) {
                    *room = &game.rooms[i];
                    break;
                }
            }
            // Solo crear sala nueva si no hay ninguna esperando
            if (*room == NULL) {
                *room = create_room(&game);
                if (*room == NULL) {
                    snprintf(response, BUFFER_SIZE, "500 SERVER_ERROR NO_ROOMS_AVAILABLE\n");
                    pthread_mutex_unlock(&game_mutex);
                    return;
                }
            }
        } else {
            *room = find_room(&game, sala_id);
            if (*room == NULL) {
                snprintf(response, BUFFER_SIZE, "404 NOT_FOUND ROOM_NOT_FOUND\n");
                pthread_mutex_unlock(&game_mutex);
                return;
            }
        }

        // Rol hardcodeado por ahora (después viene de LDAP)
        player->role = ((*room)->player_count % 2 == 0) ? ATTACKER : DEFENDER;

        add_player_to_room(*room, player);

        char role_str[16];
        strcpy(role_str, player->role == ATTACKER ? "ATTACKER" : "DEFENDER");

        // Si es defensor, incluir posiciones de recursos en la respuesta
        if (player->role == DEFENDER) {
            // Construir string de recursos: RESOURCES:x1,y1;x2,y2;...
            char resources_str[256] = {0};
            char temp[32];
            for (int i = 0; i < MAX_RESOURCES; i++) {
                if (i > 0) strncat(resources_str, ";", sizeof(resources_str) - strlen(resources_str) - 1);
                snprintf(temp, sizeof(temp), "%d,%d",
                         (*room)->resources[i].x,
                         (*room)->resources[i].y);
                strncat(resources_str, temp, sizeof(resources_str) - strlen(resources_str) - 1);
            }
            snprintf(response, BUFFER_SIZE,
                     "200 OK JOINED ROOM:%d ROLE:%s POS:%d,%d RESOURCES:%s\n",
                     (*room)->id, role_str, player->x, player->y, resources_str);
        } else {
            snprintf(response, BUFFER_SIZE,
                     "200 OK JOINED ROOM:%d ROLE:%s POS:%d,%d\n",
                     (*room)->id, role_str, player->x, player->y);
        }

        // Notificar a los demás
        char notify[BUFFER_SIZE];
        snprintf(notify, BUFFER_SIZE, "NOTIFY PLAYER_JOINED USERNAME:%s ROLE:%s\n",
                 player->username, role_str);
        notify_room(*room, player, notify);

        pthread_mutex_unlock(&game_mutex);

    // MOVE <direccion>
    } else if (strcmp(cmd, "MOVE") == 0) {
        if (*room == NULL) {
            snprintf(response, BUFFER_SIZE, "409 CONFLICT NOT_IN_ROOM\n");
            return;
        }
        pthread_mutex_lock(&game_mutex);
        process_move(player, *room, param1, response);
        pthread_mutex_unlock(&game_mutex);

    // SCAN
    } else if (strcmp(cmd, "SCAN") == 0) {
        if (*room == NULL) {
            snprintf(response, BUFFER_SIZE, "409 CONFLICT NOT_IN_ROOM\n");
            return;
        }
        pthread_mutex_lock(&game_mutex);
        process_scan(player, *room, response);
        pthread_mutex_unlock(&game_mutex);

    // ATTACK <recurso_id>
    } else if (strcmp(cmd, "ATTACK") == 0) {
        if (*room == NULL) {
            snprintf(response, BUFFER_SIZE, "409 CONFLICT NOT_IN_ROOM\n");
            return;
        }
        pthread_mutex_lock(&game_mutex);
        int resource_id = atoi(param1);
        process_attack(player, *room, resource_id, response);

        // Si el ataque fue exitoso, notificar a defensores
        if (strncmp(response, "200", 3) == 0) {
            char notify[BUFFER_SIZE];
            snprintf(notify, BUFFER_SIZE, "NOTIFY ATTACK_STARTED RESOURCE_ID:%d ATTACKER:%s\n",
                     resource_id, player->username);
            notify_room(*room, player, notify);
        }
        pthread_mutex_unlock(&game_mutex);

    // DEFEND <recurso_id>
    } else if (strcmp(cmd, "DEFEND") == 0) {
        if (*room == NULL) {
            snprintf(response, BUFFER_SIZE, "409 CONFLICT NOT_IN_ROOM\n");
            return;
        }
        pthread_mutex_lock(&game_mutex);
        int resource_id = atoi(param1);
        process_defend(player, *room, resource_id, response);

        // Si la defensa fue exitosa, notificar a todos
        if (strncmp(response, "200", 3) == 0) {
            char notify[BUFFER_SIZE];
            snprintf(notify, BUFFER_SIZE, "NOTIFY ATTACK_MITIGATED RESOURCE_ID:%d DEFENDER:%s\n",
                     resource_id, player->username);
            notify_room(*room, player, notify);
        }
        pthread_mutex_unlock(&game_mutex);

    // STATUS
    } else if (strcmp(cmd, "STATUS") == 0) {
        if (*room == NULL) {
            snprintf(response, BUFFER_SIZE, "409 CONFLICT NOT_IN_ROOM\n");
            return;
        }
        char role_str[16];
        strcpy(role_str, player->role == ATTACKER ? "ATTACKER" : "DEFENDER");
        snprintf(response, BUFFER_SIZE, "200 OK STATUS ROLE:%s POS:%d,%d ROOM:%d PLAYERS:%d\n",
                 role_str, player->x, player->y, (*room)->id, (*room)->player_count);

    // QUIT
    } else if (strcmp(cmd, "QUIT") == 0) {
        if (*room != NULL) {
            pthread_mutex_lock(&game_mutex);
            char notify[BUFFER_SIZE];
            snprintf(notify, BUFFER_SIZE, "NOTIFY PLAYER_LEFT USERNAME:%s\n", player->username);
            notify_room(*room, player, notify);
            remove_player(*room, player);
            pthread_mutex_unlock(&game_mutex);
        }
        snprintf(response, BUFFER_SIZE, "200 OK GOODBYE\n");

    // Comando desconocido
    } else {
        snprintf(response, BUFFER_SIZE, "400 BAD_REQUEST UNKNOWN_COMMAND\n");
    }
}

// ── Hilo por cliente ─────────────────────────────────
typedef struct {
    int socket_fd;
    struct sockaddr_in address;
} ClientArgs;

void* handle_client(void* arg) {
    ClientArgs* args = (ClientArgs*)arg;
    int client_fd = args->socket_fd;
    char client_ip[INET_ADDRSTRLEN];
    int client_port = ntohs(args->address.sin_port);
    inet_ntop(AF_INET, &args->address.sin_addr, client_ip, INET_ADDRSTRLEN);
    free(arg);

    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    Room* room = NULL;

    // Crear jugador para este cliente
    Player player;
    memset(&player, 0, sizeof(Player));
    player.socket_fd = client_fd;
    player.active = 1;

    printf("Cliente conectado: %s:%d\n", client_ip, client_port);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        memset(response, 0, BUFFER_SIZE);

        int bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

        // Cliente desconectado
        if (bytes <= 0) {
            printf("Cliente desconectado: %s:%d\n", client_ip, client_port);
            if (room != NULL) {
                pthread_mutex_lock(&game_mutex);
                char notify[BUFFER_SIZE];
                snprintf(notify, BUFFER_SIZE, "NOTIFY PLAYER_LEFT USERNAME:%s\n", player.username);
                notify_room(room, &player, notify);
                remove_player(room, &player);
                pthread_mutex_unlock(&game_mutex);
            }
            break;
        }

        handle_client_message(&player, &room, buffer, response, client_ip, client_port);
        log_message(client_ip, client_port, "RESPONSE", response);
        send(client_fd, response, strlen(response), 0);
    }

    close(client_fd);
    return NULL;
}

// ── Main ─────────────────────────────────────────────
int main(int argc, char* argv[]) {

    if (argc != 3) {
        printf("Uso: ./server <puerto> <archivo_logs>\n");
        printf("Ejemplo: ./server 8080 logs/server.log\n");
        exit(1);
    }

    int port = atoi(argv[1]);
    char* log_path = argv[2];

    log_file = fopen(log_path, "a");
    if (!log_file) {
        perror("Error abriendo archivo de logs");
        exit(1);
    }

    init_game(&game);

    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Error creando socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Error en bind");
        exit(1);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Error en listen");
        exit(1);
    }

    printf("Servidor GCP corriendo en el puerto %d...\n", port);
    printf("Logs guardandose en: %s\n", log_path);

    while (1) {
        ClientArgs* args = malloc(sizeof(ClientArgs));
        args->socket_fd = accept(server_fd, (struct sockaddr*)&args->address, (socklen_t*)&addrlen);

        if (args->socket_fd < 0) {
            perror("Error en accept");
            free(args);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, args);
        pthread_detach(thread);
    }

    fclose(log_file);
    close(server_fd);
    return 0;
}
