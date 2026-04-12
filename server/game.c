#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "game.h"
#include <sys/socket.h>

// Mutex global del juego (definido en server.c)
extern pthread_mutex_t game_mutex;

// Inicializa el estado global del juego
void init_game(GameData* game) {
    game->room_count = 0;
    for (int i = 0; i < MAX_ROOMS; i++) {
        game->rooms[i].id = -1;
        game->rooms[i].state = WAITING;
        game->rooms[i].player_count = 0;
        for (int j = 0; j < MAX_PLAYERS; j++) {
            game->rooms[i].players[j] = NULL;
        }
    }
}

// Crea una sala nueva y genera los recursos críticos en posiciones aleatorias
Room* create_room(GameData* game) {
    if (game->room_count >= MAX_ROOMS) return NULL;

    Room* room = &game->rooms[game->room_count];
    room->id = game->room_count + 1;
    room->state = WAITING;
    room->player_count = 0;
    room->successful_defenses = 0;

    // Generar posiciones aleatorias para los recursos críticos
    // (srand() se llama una sola vez en main, no aqui)
    for (int i = 0; i < MAX_RESOURCES; i++) {
        room->resources[i].id = i + 1;
        room->resources[i].x = rand() % MAP_SIZE;
        room->resources[i].y = rand() % MAP_SIZE;
        room->resources[i].state = SAFE;
        room->resources[i].attack_started_at = 0;
        room->resources[i].timer_active = 0;
        room->resources[i].timer_token = 0;
        room->resources[i].room_id = room->id;
    }

    game->room_count++;
    return room;
}

// Busca una sala por su ID
Room* find_room(GameData* game, int room_id) {
    for (int i = 0; i < game->room_count; i++) {
        if (game->rooms[i].id == room_id) {
            return &game->rooms[i];
        }
    }
    return NULL;
}

// Agrega un jugador a una sala
int add_player_to_room(Room* room, Player* player) {
    if (room->player_count >= MAX_PLAYERS) return -1;

    room->players[room->player_count] = player;
    player->room_id = room->id;

    // Posición inicial aleatoria
    player->x = rand() % MAP_SIZE;
    player->y = rand() % MAP_SIZE;

    room->player_count++;

    // Si hay al menos 1 atacante y 1 defensor, arranca la partida
    int attackers = 0, defenders = 0;
    for (int i = 0; i < room->player_count; i++) {
        if (room->players[i]->role == ATTACKER) attackers++;
        if (room->players[i]->role == DEFENDER) defenders++;
    }
    if (attackers >= 1 && defenders >= 1) {
        room->state = RUNNING;
    }

    return 0;
}

// Elimina un jugador de una sala
void remove_player(Room* room, Player* player) {
    for (int i = 0; i < room->player_count; i++) {
        if (room->players[i] == player) {
            // Corre los jugadores restantes una posición hacia atrás
            for (int j = i; j < room->player_count - 1; j++) {
                room->players[j] = room->players[j + 1];
            }
            room->players[room->player_count - 1] = NULL;
            room->player_count--;
            player->active = 0;
            return;
        }
    }
}

// Manda un mensaje a todos los jugadores de una sala excepto al que lo originó
void notify_room(Room* room, Player* sender, const char* message) {
    for (int i = 0; i < room->player_count; i++) {
        Player* p = room->players[i];
        if (p != sender && p->active) {
            send(p->socket_fd, message, strlen(message), 0);
        }
    }
}

// Manda un mensaje a TODOS los jugadores de la sala (incluido el que lo originó)
void notify_all(Room* room, const char* message) {
    for (int i = 0; i < room->player_count; i++) {
        Player* p = room->players[i];
        if (p && p->active) {
            send(p->socket_fd, message, strlen(message), 0);
        }
    }
}

// Retorna 1 si TODOS los recursos están COMPROMISED (ganan atacantes).
// Un recurso solo queda COMPROMISED si su temporizador venció sin defensa.
int check_attackers_win(Room* room) {
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (room->resources[i].state != COMPROMISED) return 0;
    }
    return 1;
}

// Retorna 1 si los defensores ya acumularon suficientes defensas exitosas.
// El umbral está en DEFENDERS_WIN_THRESHOLD (game.h).
int check_defenders_win(Room* room) {
    return room->successful_defenses >= DEFENDERS_WIN_THRESHOLD;
}

// Procesa el comando MOVE
int process_move(Player* player, Room* room, const char* direction, char* response) {
    if (room->state != RUNNING) {
        snprintf(response, BUFFER_SIZE, "409 CONFLICT GAME_NOT_RUNNING\n");
        return -1;
    }

    int new_x = player->x;
    int new_y = player->y;

    if      (strcmp(direction, "UP")    == 0) new_y--;
    else if (strcmp(direction, "DOWN")  == 0) new_y++;
    else if (strcmp(direction, "LEFT")  == 0) new_x--;
    else if (strcmp(direction, "RIGHT") == 0) new_x++;
    else {
        snprintf(response, BUFFER_SIZE, "400 BAD_REQUEST INVALID_DIRECTION\n");
        return -1;
    }

    // Verificar límites del mapa
    if (new_x < 0 || new_x >= MAP_SIZE || new_y < 0 || new_y >= MAP_SIZE) {
        snprintf(response, BUFFER_SIZE, "409 CONFLICT OUT_OF_BOUNDS\n");
        return -1;
    }

    player->x = new_x;
    player->y = new_y;
    snprintf(response, BUFFER_SIZE, "200 OK MOVE_ACCEPTED POS:%d,%d\n", player->x, player->y);
    return 0;
}

// Procesa el comando SCAN (solo atacantes)
int process_scan(Player* player, Room* room, char* response) {
    if (player->role != ATTACKER) {
        snprintf(response, BUFFER_SIZE, "403 FORBIDDEN ACTION_NOT_ALLOWED_FOR_ROLE\n");
        return -1;
    }
    if (room->state != RUNNING) {
        snprintf(response, BUFFER_SIZE, "409 CONFLICT GAME_NOT_RUNNING\n");
        return -1;
    }

    // Revisar si hay un recurso en la posición actual
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (room->resources[i].x == player->x && room->resources[i].y == player->y) {
            snprintf(response, BUFFER_SIZE, "201 FOUND RESOURCE_ID:%d POS:%d,%d\n",
                     room->resources[i].id, player->x, player->y);
            return 0;
        }
    }

    snprintf(response, BUFFER_SIZE, "200 OK NOTHING_FOUND POS:%d,%d\n", player->x, player->y);
    return 0;
}

// Procesa el comando ATTACK (solo atacantes)
int process_attack(Player* player, Room* room, int resource_id, char* response) {
    if (player->role != ATTACKER) {
        snprintf(response, BUFFER_SIZE, "403 FORBIDDEN ACTION_NOT_ALLOWED_FOR_ROLE\n");
        return -1;
    }
    if (room->state != RUNNING) {
        snprintf(response, BUFFER_SIZE, "409 CONFLICT GAME_NOT_RUNNING\n");
        return -1;
    }

    // Buscar el recurso
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (room->resources[i].id == resource_id) {
            // Verificar que el atacante esté en la casilla del recurso
            if (room->resources[i].x != player->x || room->resources[i].y != player->y) {
                snprintf(response, BUFFER_SIZE, "409 CONFLICT NOT_ON_RESOURCE\n");
                return -1;
            }
            // No se puede re-atacar algo ya comprometido o bajo ataque activo
            if (room->resources[i].state == COMPROMISED) {
                snprintf(response, BUFFER_SIZE,
                         "409 CONFLICT RESOURCE_ALREADY_COMPROMISED\n");
                return -1;
            }
            if (room->resources[i].state == UNDER_ATTACK) {
                snprintf(response, BUFFER_SIZE,
                         "409 CONFLICT RESOURCE_ALREADY_UNDER_ATTACK\n");
                return -1;
            }
            // Marcar ataque y arrancar temporizador
            room->resources[i].state = UNDER_ATTACK;
            room->resources[i].attack_started_at = time(NULL);
            start_attack_timer(room, &room->resources[i]);
            snprintf(response, BUFFER_SIZE,
                     "200 OK ATTACK_LAUNCHED RESOURCE_ID:%d TIMEOUT:%d\n",
                     resource_id, ATTACK_TIMEOUT);
            return 0;
        }
    }

    snprintf(response, BUFFER_SIZE, "404 NOT_FOUND RESOURCE_NOT_FOUND\n");
    return -1;
}

// Procesa el comando DEFEND (solo defensores)
int process_defend(Player* player, Room* room, int resource_id, char* response) {
    if (player->role != DEFENDER) {
        snprintf(response, BUFFER_SIZE, "403 FORBIDDEN ACTION_NOT_ALLOWED_FOR_ROLE\n");
        return -1;
    }
    if (room->state != RUNNING) {
        snprintf(response, BUFFER_SIZE, "409 CONFLICT GAME_NOT_RUNNING\n");
        return -1;
    }

    // Buscar el recurso
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (room->resources[i].id == resource_id) {
            if (room->resources[i].state == COMPROMISED) {
                snprintf(response, BUFFER_SIZE,
                         "409 CONFLICT RESOURCE_ALREADY_COMPROMISED\n");
                return -1;
            }
            if (room->resources[i].state != UNDER_ATTACK) {
                snprintf(response, BUFFER_SIZE,
                         "409 CONFLICT RESOURCE_NOT_UNDER_ATTACK\n");
                return -1;
            }
            // Invalidar el temporizador activo y volver a SAFE
            room->resources[i].timer_token++;   // cualquier timer viejo queda inválido
            room->resources[i].state = SAFE;
            room->resources[i].attack_started_at = 0;
            room->successful_defenses++;
            snprintf(response, BUFFER_SIZE,
                     "200 OK DEFENSE_SUCCESS RESOURCE_ID:%d DEFENSES:%d/%d\n",
                     resource_id, room->successful_defenses, DEFENDERS_WIN_THRESHOLD);
            return 0;
        }
    }

    snprintf(response, BUFFER_SIZE, "404 NOT_FOUND RESOURCE_NOT_FOUND\n");
    return -1;
}

// ── Temporizador de ataque ────────────────────────────
// Argumento que se pasa al hilo temporizador
typedef struct {
    Room* room;
    Resource* resource;
    int token;       // snapshot del timer_token al lanzar el hilo
} AttackTimerArgs;

static void* attack_timer_thread(void* arg) {
    AttackTimerArgs* args = (AttackTimerArgs*)arg;
    Room* room = args->room;
    Resource* res = args->resource;
    int my_token = args->token;
    free(args);

    sleep(ATTACK_TIMEOUT);

    pthread_mutex_lock(&game_mutex);

    // Si el token cambió, alguien defendió o hubo un nuevo ataque: salimos.
    if (res->timer_token != my_token) {
        pthread_mutex_unlock(&game_mutex);
        return NULL;
    }

    // Si la sala ya terminó, no hacemos nada.
    if (room->state != RUNNING) {
        pthread_mutex_unlock(&game_mutex);
        return NULL;
    }

    // Sigue bajo ataque sin mitigar → queda comprometido
    if (res->state == UNDER_ATTACK) {
        res->state = COMPROMISED;
        res->timer_active = 0;

        char notify[BUFFER_SIZE];
        snprintf(notify, BUFFER_SIZE,
                 "NOTIFY RESOURCE_DOWN RESOURCE_ID:%d\n", res->id);
        notify_all(room, notify);

        // ¿Ganaron los atacantes?
        if (check_attackers_win(room)) {
            room->state = FINISHED;
            char gameover[BUFFER_SIZE];
            snprintf(gameover, BUFFER_SIZE,
                     "NOTIFY GAME_OVER RESULT:ATTACKERS_WIN\n");
            notify_all(room, gameover);
        }
    }

    pthread_mutex_unlock(&game_mutex);
    return NULL;
}

void start_attack_timer(Room* room, Resource* resource) {
    resource->timer_token++;
    resource->timer_active = 1;

    AttackTimerArgs* args = malloc(sizeof(AttackTimerArgs));
    if (!args) return;
    args->room = room;
    args->resource = resource;
    args->token = resource->timer_token;

    pthread_t tid;
    if (pthread_create(&tid, NULL, attack_timer_thread, args) != 0) {
        free(args);
        resource->timer_active = 0;
        return;
    }
    pthread_detach(tid);
}
