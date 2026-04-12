#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "game.h"
#include <sys/socket.h>

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

    // Generar posiciones aleatorias para los recursos críticos
    // (srand() se llama una sola vez en main, no aqui)
    for (int i = 0; i < MAX_RESOURCES; i++) {
        room->resources[i].id = i + 1;
        room->resources[i].x = rand() % MAP_SIZE;
        room->resources[i].y = rand() % MAP_SIZE;
        room->resources[i].under_attack = 0;
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

// Retorna 1 si todos los recursos están bajo ataque (ganan atacantes)
int check_attackers_win(Room* room) {
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (!room->resources[i].under_attack) return 0;
    }
    return 1;
}

// Retorna 1 si ningún recurso está bajo ataque (ganan defensores)
int check_defenders_win(Room* room) {
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (room->resources[i].under_attack) return 0;
    }
    return 1;
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
            room->resources[i].under_attack = 1;
            snprintf(response, BUFFER_SIZE, "200 OK ATTACK_LAUNCHED RESOURCE_ID:%d\n", resource_id);
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
            if (!room->resources[i].under_attack) {
                snprintf(response, BUFFER_SIZE, "409 CONFLICT RESOURCE_NOT_UNDER_ATTACK\n");
                return -1;
            }
            room->resources[i].under_attack = 0;
            snprintf(response, BUFFER_SIZE, "200 OK DEFENSE_SUCCESS RESOURCE_ID:%d\n", resource_id);
            return 0;
        }
    }

    snprintf(response, BUFFER_SIZE, "404 NOT_FOUND RESOURCE_NOT_FOUND\n");
    return -1;
}
