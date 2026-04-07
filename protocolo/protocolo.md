# GCP - Game Communication Protocol

**Version:** 1.0  
**Fecha:** Marzo 2026  
**Autores:** Jean Carlo Ardila Acevedo - Valentina Zapata Acosta

---

## 1. Vision General

GCP es un protocolo de capa de aplicacion disenado para soportar un juego multijugador
en tiempo real donde los participantes asumen roles de Atacante o Defensor dentro de
un simulador de centro de datos.

El modelo de funcionamiento es **cliente-servidor**:
- El **servidor** centraliza el estado del juego, valida acciones y notifica eventos.
- Los **clientes** envian acciones y reciben respuestas y notificaciones del servidor.

### Por que TCP y no UDP?

El protocolo opera sobre **TCP** (SOCK_STREAM) por las siguientes razones:

- Cada accion del juego depende del estado anterior. Si un MOVE se pierde, el servidor
  y el cliente tendrian posiciones distintas y el juego se romperia.
- TCP garantiza que los mensajes lleguen en orden y sin perdidas, lo cual es critico
  para un juego donde el estado es acumulativo.
- UDP seria adecuado si la velocidad importara mas que la precision (por ejemplo,
  streaming de video), pero en este juego la consistencia del estado es lo primero.

El protocolo es de **codificacion tipo texto**, con mensajes en formato legible separados
por salto de linea (`\n`). Cada conexion TCP se mantiene abierta durante toda la partida
(protocolo con estado / stateful).

---

## 2. Especificacion del Servicio

### 2.1 Roles

| Rol | Descripcion |
|---|---|
| ATTACKER | Se mueve por el plano explorando hasta encontrar un recurso critico y atacarlo. No conoce la ubicacion de los recursos al inicio. |
| DEFENDER | Conoce la ubicacion de todos los recursos criticos desde que entra a la sala. Debe moverse y mitigar los ataques. |

### 2.2 Vocabulario de Mensajes (Comandos)

Estos son todos los mensajes que pueden intercambiarse entre cliente y servidor:

| Comando | Direccion | Quien lo usa | Descripcion |
|---|---|---|---|
| `JOIN` | Cliente -> Servidor | Ambos | Solicita unirse a una partida activa o crear una nueva. |
| `MOVE` | Cliente -> Servidor | Ambos | Mueve al jugador en una direccion dentro del plano. |
| `SCAN` | Cliente -> Servidor | Solo ATTACKER | Explora la casilla actual buscando recursos criticos. |
| `ATTACK` | Cliente -> Servidor | Solo ATTACKER | Lanza un ataque sobre el recurso critico encontrado. |
| `DEFEND` | Cliente -> Servidor | Solo DEFENDER | Ejecuta la mitigacion sobre un recurso bajo ataque. |
| `STATUS` | Cliente -> Servidor | Ambos | Consulta el estado actual del jugador y la partida. |
| `QUIT` | Cliente -> Servidor | Ambos | El jugador abandona la partida. |
| `NOTIFY` | Servidor -> Cliente | - | El servidor notifica un evento espontaneo al cliente. |

### 2.3 Descripcion de campos por mensaje

| Campo | Tipo | Tamano maximo | Valores posibles |
|---|---|---|---|
| `sala_id` | Entero | 10 digitos | 0 (crear nueva) o ID existente |
| `username` | Texto | 20 caracteres | Sin espacios |
| `direccion` | Texto | 5 caracteres | UP, DOWN, LEFT, RIGHT |
| `recurso_id` | Entero | 10 digitos | 1 o 2 (hay 2 recursos por sala) |
| `codigo` | Entero | 3 digitos | 200, 201, 400, 403, 404, 409, 500 |

---

## 3. Formato de Mensajes

Todos los mensajes siguen esta estructura:

```text
COMANDO PARAM1 PARAM2 ... \n
```

Cada mensaje termina con un salto de linea (`\n`). Los campos se separan por un espacio.

### 3.1 Mensajes del Cliente al Servidor

#### JOIN

```text
JOIN <sala_id> <username>
```

- `sala_id`: ID numerico de la sala. Usar `0` para unirse a una sala en espera o crear una nueva.
- `username`: Nombre del jugador (maximo 20 caracteres, sin espacios). Debe existir en el servidor de identidad.

**Ejemplo:**

```text
JOIN 0 hacker99
JOIN 3 defender01
```

---

#### MOVE

```text
MOVE <direccion>
```

- `direccion`: `UP`, `DOWN`, `LEFT`, `RIGHT`.
- Solo funciona cuando la partida esta en estado RUNNING.

**Ejemplo:**

```text
MOVE UP
MOVE RIGHT
```

---

#### SCAN

```text
SCAN
```

No lleva parametros. El servidor revisa si hay un recurso critico en la casilla exacta donde esta el atacante.
Solo disponible para ATTACKER.

---

#### ATTACK

```text
ATTACK <recurso_id>
```

- `recurso_id`: ID del recurso critico a atacar. El atacante debe estar parado exactamente sobre ese recurso.
- Solo disponible para ATTACKER.

**Ejemplo:**

```text
ATTACK 1
ATTACK 2
```

---

#### DEFEND

```text
DEFEND <recurso_id>
```

- `recurso_id`: ID del recurso critico a defender. El recurso debe estar bajo ataque en ese momento.
- Solo disponible para DEFENDER.

**Ejemplo:**

```text
DEFEND 1
```

---

#### STATUS

```text
STATUS
```

El servidor responde con la posicion actual del jugador, su rol, la sala y cuantos jugadores hay.

---

#### QUIT

```text
QUIT
```

El jugador notifica al servidor que abandona la partida. El servidor elimina al jugador de la sala
y notifica a los demas.

### 3.2 Mensajes del Servidor al Cliente

Todas las respuestas del servidor siguen este formato:

```text
<codigo> <estado> <detalle> [CLAVE:VALOR ...]
```

| Codigo | Significado | Cuando ocurre |
|---|---|---|
| `200` | OK | Accion ejecutada correctamente. |
| `201` | FOUND | Se encontro un recurso critico al hacer SCAN. |
| `400` | BAD_REQUEST | Comando desconocido o mal formado. |
| `403` | FORBIDDEN | Accion no permitida para ese rol, o usuario no encontrado. |
| `404` | NOT_FOUND | Sala o recurso con ID inexistente. |
| `409` | CONFLICT | Accion invalida en el estado actual (juego no iniciado, fuera del mapa, etc). |
| `500` | SERVER_ERROR | Error interno del servidor (por ejemplo, no hay salas disponibles). |

**Ejemplos de respuestas:**

```text
200 OK MOVE_ACCEPTED POS:5,7
200 OK JOINED ROOM:1 ROLE:ATTACKER POS:3,4
200 OK JOINED ROOM:1 ROLE:DEFENDER POS:3,4 RESOURCES:5,14;16,14
201 FOUND RESOURCE_ID:2 POS:5,7
403 FORBIDDEN ACTION_NOT_ALLOWED_FOR_ROLE
404 NOT_FOUND ROOM_NOT_FOUND
409 CONFLICT GAME_NOT_RUNNING
400 BAD_REQUEST UNKNOWN_COMMAND
```

---

#### NOTIFY (Servidor -> Cliente, sin solicitud previa)

El servidor puede enviar notificaciones en cualquier momento sin que el cliente las haya pedido.
Esto es posible porque la conexion TCP se mantiene abierta durante toda la partida.

```text
NOTIFY <tipo_evento> <datos>
```

| Tipo de evento | A quien se envia | Cuando |
|---|---|---|
| `ATTACK_STARTED` | A todos en la sala excepto al atacante | Cuando un atacante lanza ATTACK exitoso. |
| `ATTACK_MITIGATED` | A todos en la sala excepto al defensor | Cuando un defensor ejecuta DEFEND exitoso. |
| `GAME_OVER` | A todos en la sala | Cuando la partida termina (ganan atacantes o defensores). |
| `PLAYER_JOINED` | A todos en la sala excepto el que entro | Cuando un jugador hace JOIN exitoso. |
| `PLAYER_LEFT` | A todos en la sala | Cuando un jugador hace QUIT o se desconecta. |

**Ejemplos:**

```text
NOTIFY ATTACK_STARTED RESOURCE_ID:1 ATTACKER:hacker99
NOTIFY ATTACK_MITIGATED RESOURCE_ID:1 DEFENDER:defender01
NOTIFY GAME_OVER RESULT:ATTACKERS_WIN
NOTIFY GAME_OVER RESULT:DEFENDERS_WIN
NOTIFY PLAYER_JOINED USERNAME:bob ROLE:DEFENDER
NOTIFY PLAYER_LEFT USERNAME:hacker99
```

---

## 4. Reglas de Procedimiento

### 4.1 Maquina de estados de la partida

Una sala puede estar en tres estados posibles:

```
            JOIN (primer jugador)
                    |
                    v
              [ WAITING ]
                    |
         Llega al menos 1 ATTACKER
         y al menos 1 DEFENDER
                    |
                    v
              [ RUNNING ]  <---- aqui se juega
                    |
         Todos los recursos atacados    O    todos los ataques mitigados
                    |
                    v
              [ FINISHED ]
```

| Estado | Que significa | Que comandos acepta |
|---|---|---|
| WAITING | Esperando jugadores | JOIN |
| RUNNING | Partida en curso | MOVE, SCAN, ATTACK, DEFEND, STATUS, QUIT |
| FINISHED | Partida terminada | QUIT |

Si un cliente intenta MOVE, SCAN, ATTACK o DEFEND con la sala en WAITING o FINISHED,
el servidor responde `409 CONFLICT GAME_NOT_RUNNING`.

### 4.2 Maquina de estados de la conexion del cliente

```
  Conectado TCP
       |
       v
  [ SIN SALA ]  -- JOIN exitoso -->  [ EN SALA / WAITING ]
                                             |
                                     partida inicia
                                             |
                                             v
                                     [ EN SALA / RUNNING ]
                                             |
                              QUIT o desconexion abrupta
                                             |
                                             v
                                     [ DESCONECTADO ]
```

### 4.3 Condiciones de victoria

- **Ganan los ATTACKER:** todos los recursos criticos de la sala quedan en estado `under_attack = 1`
  al mismo tiempo. El servidor envia `NOTIFY GAME_OVER RESULT:ATTACKERS_WIN` a todos.

- **Ganan los DEFENDER:** se mitiga un ataque y ninguno de los recursos queda bajo ataque.
  El servidor envia `NOTIFY GAME_OVER RESULT:DEFENDERS_WIN` a todos.

### 4.4 Restricciones por rol

- Solo el **ATTACKER** puede usar: `SCAN`, `ATTACK`.
- Solo el **DEFENDER** puede usar: `DEFEND`.
- Ambos pueden usar: `MOVE`, `STATUS`, `QUIT`.
- Cualquier intento de usar un comando del rol contrario retorna `403 FORBIDDEN`.

### 4.5 Restricciones del mapa

- El plano es una cuadricula de **20x20** (posiciones de 0,0 a 19,19).
- Si un jugador intenta moverse fuera del limite, el servidor retorna `409 CONFLICT OUT_OF_BOUNDS`.
- Los recursos criticos tienen posicion aleatoria fija al inicio de cada partida.
- El ATTACKER debe estar en la casilla exacta del recurso para hacer ATTACK.

### 4.6 Manejo de errores y excepciones

| Situacion | Comportamiento |
|---|---|
| Comando desconocido | Servidor responde `400 BAD_REQUEST UNKNOWN_COMMAND`. La conexion sigue abierta. |
| Comando fuera de rol | Servidor responde `403 FORBIDDEN ACTION_NOT_ALLOWED_FOR_ROLE`. |
| Usuario no existe en auth server | Servidor responde `403 FORBIDDEN USER_NOT_FOUND`. |
| Sala no existe | Servidor responde `404 NOT_FOUND ROOM_NOT_FOUND`. |
| Juego no iniciado | Servidor responde `409 CONFLICT GAME_NOT_RUNNING`. |
| Movimiento fuera del mapa | Servidor responde `409 CONFLICT OUT_OF_BOUNDS`. |
| Atacar sin estar sobre el recurso | Servidor responde `409 CONFLICT NOT_ON_RESOURCE`. |
| Defender recurso sin ataque | Servidor responde `409 CONFLICT RESOURCE_NOT_UNDER_ATTACK`. |
| Cliente se desconecta abruptamente | Servidor elimina al jugador de la sala y notifica `NOTIFY PLAYER_LEFT`. |
| Fallo en resolucion DNS | El cliente muestra el error pero no cierra la aplicacion. |
| Auth server no disponible | Servidor responde `403 FORBIDDEN USER_NOT_FOUND` y loguea el error. |

---

## 5. Ejemplos de Flujo

### Ejemplo 1: Atacante encuentra y ataca un recurso

```text
Cliente (ATTACKER)                        Servidor
--------------------------------------------------
JOIN 0 hacker99               ->
                              <- 200 OK JOINED ROOM:1 ROLE:ATTACKER POS:3,4
MOVE RIGHT                    ->
                              <- 200 OK MOVE_ACCEPTED POS:4,4
SCAN                          ->
                              <- 200 OK NOTHING_FOUND POS:4,4
MOVE DOWN                     ->
                              <- 200 OK MOVE_ACCEPTED POS:4,5
SCAN                          ->
                              <- 201 FOUND RESOURCE_ID:1 POS:4,5
ATTACK 1                      ->
                              <- 200 OK ATTACK_LAUNCHED RESOURCE_ID:1
                              <- [A defensores] NOTIFY ATTACK_STARTED RESOURCE_ID:1 ATTACKER:hacker99
```

### Ejemplo 2: Defensor mitiga el ataque y gana

```text
Cliente (DEFENDER)                        Servidor
--------------------------------------------------
JOIN 1 defender01             ->
                              <- 200 OK JOINED ROOM:1 ROLE:DEFENDER POS:10,10 RESOURCES:4,5;16,14
                              <- NOTIFY ATTACK_STARTED RESOURCE_ID:1 ATTACKER:hacker99
DEFEND 1                      ->
                              <- 200 OK DEFENSE_SUCCESS RESOURCE_ID:1
                              <- [A todos] NOTIFY ATTACK_MITIGATED RESOURCE_ID:1 DEFENDER:defender01
                              <- NOTIFY GAME_OVER RESULT:DEFENDERS_WIN
```

### Ejemplo 3: Comando fuera de rol

```text
Cliente (DEFENDER)                        Servidor
--------------------------------------------------
SCAN                          ->
                              <- 403 FORBIDDEN ACTION_NOT_ALLOWED_FOR_ROLE
```

### Ejemplo 4: Mensaje desconocido

```text
Cliente                                   Servidor
--------------------------------------------------
ATACAR servidor1              ->
                              <- 400 BAD_REQUEST UNKNOWN_COMMAND
```

---

## 6. Logging

El servidor registra cada peticion y respuesta con el siguiente formato:

```text
[TIMESTAMP] [IP:PUERTO] REQUEST: <mensaje_recibido>
[TIMESTAMP] [IP:PUERTO] RESPONSE: <mensaje_enviado>
```

**Ejemplo:**

```text
[2026-03-13 19:05:32] [192.168.56.101:54231] REQUEST: MOVE RIGHT
[2026-03-13 19:05:32] [192.168.56.101:54231] RESPONSE: 200 OK MOVE_ACCEPTED POS:6,3
[2026-03-13 19:05:45] [192.168.56.101:54231] REQUEST: ATTACK 1
[2026-03-13 19:05:45] [192.168.56.101:54231] RESPONSE: 200 OK ATTACK_LAUNCHED RESOURCE_ID:1
```

Los logs se guardan simultaneamente en consola y en el archivo indicado al arrancar el servidor:

```bash
./server 8080 logs/server.log
```
