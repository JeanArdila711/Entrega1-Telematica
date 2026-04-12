#ifndef GAME_H
#define GAME_H

#include <pthread.h>
#include <time.h>

#define MAX_PLAYERS 10
#define MAX_ROOMS   5
#define MAP_SIZE    20
#define MAX_RESOURCES 2
#define BUFFER_SIZE 1024
#define MAX_USERNAME 21

// Tiempo (segundos) que los defensores tienen para mitigar un ataque
// antes de que el recurso quede comprometido de forma permanente.
#define ATTACK_TIMEOUT 30

// Cantidad de defensas exitosas necesarias para que ganen los defensores.
// Se fija en 2 porque hay 2 recursos: los defensores deben probar que
// pueden contener ataques a ambos antes de declararse ganadores.
#define DEFENDERS_WIN_THRESHOLD 2

// Roles del jugador
typedef enum {
    ATTACKER,
    DEFENDER
} Role;

// Estados del juego
typedef enum {
    WAITING,   // Esperando jugadores
    RUNNING,   // Partida en curso
    FINISHED   // Partida terminada
} GameState;

// Estado de un recurso crítico
typedef enum {
    SAFE,            // Sin ataque activo
    UNDER_ATTACK,    // Siendo atacado, defensor aún puede mitigar
    COMPROMISED      // Ataque consumado: el timer venció
} ResourceState;

// Recurso crítico en el mapa
typedef struct {
    int id;
    int x, y;
    ResourceState state;
    time_t attack_started_at;    // timestamp de inicio del ataque actual
    int timer_active;            // 1 mientras el hilo temporizador esté vivo
    int timer_token;             // se incrementa para invalidar timers antiguos
    int room_id;                 // para que el hilo timer sepa a qué sala pertenece
} Resource;

// Jugador
typedef struct {
    int socket_fd;          // Socket del cliente
    char username[MAX_USERNAME];
    Role role;
    int x, y;               // Posición en el mapa
    int room_id;            // Sala a la que pertenece
    int active;             // 1 si está conectado, 0 si no
} Player;

// Sala de juego
typedef struct {
    int id;
    GameState state;
    Player* players[MAX_PLAYERS];
    int player_count;
    Resource resources[MAX_RESOURCES];
    int successful_defenses;   // defensas exitosas acumuladas
} Room;

// Estado global del juego
typedef struct {
    Room rooms[MAX_ROOMS];
    int room_count;
} GameData;

// ── Declaración de server.c (usada en http_server.c) ─
int query_auth_server(const char* username, const char* password, char* role_out);

// ── Funciones de game.c ──────────────────────────────

void init_game(GameData* game);
Room* create_room(GameData* game);
Room* find_room(GameData* game, int room_id);
int add_player_to_room(Room* room, Player* player);
void remove_player(Room* room, Player* player);
void notify_room(Room* room, Player* sender, const char* message);
void notify_all(Room* room, const char* message);
int check_attackers_win(Room* room);
int check_defenders_win(Room* room);
int process_move(Player* player, Room* room, const char* direction, char* response);
int process_scan(Player* player, Room* room, char* response);
int process_attack(Player* player, Room* room, int resource_id, char* response);
int process_defend(Player* player, Room* room, int resource_id, char* response);

// Lanza el hilo temporizador para un ataque en curso.
// Si expira sin DEFEND, marca el recurso como COMPROMISED.
void start_attack_timer(Room* room, Resource* resource);

#endif
