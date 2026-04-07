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

El protocolo opera sobre **TCP** (SOCK_STREAM), lo que garantiza que todos los mensajes
lleguen en orden y sin perdidas. Esto es critico porque cada accion dentro del juego
(moverse, atacar, defender) depende del estado anterior; si se pierde un mensaje,
el estado del juego se corrompe.

El protocolo es de **codificacion tipo texto**, con mensajes en formato legible separados
por salto de linea (`\n`).

---

## 2. Especificacion del Servicio

### 2.1 Roles

| Rol | Descripcion |
|---|---|
| Atacante | Se mueve por el plano explorando hasta encontrar un recurso critico y atacarlo. |
| Defensor | Conoce la ubicacion de los recursos criticos y debe mitigar los ataques a tiempo. |

### 2.2 Vocabulario de Mensajes (Comandos)

Estos son todos los mensajes que pueden intercambiarse entre cliente y servidor:

| Comando | Direccion | Quien lo usa? | Descripcion |
|---|---|---|---|
| `JOIN` | Cliente -> Servidor | Ambos | Solicita unirse a una partida activa. |
| `MOVE` | Cliente -> Servidor | Ambos | Mueve al jugador en una direccion dentro del plano. |
| `SCAN` | Cliente -> Servidor | Atacante | Explora la casilla actual buscando recursos criticos. |
| `ATTACK` | Cliente -> Servidor | Atacante | Lanza un ataque sobre el recurso critico encontrado. |
| `DEFEND` | Cliente -> Servidor | Defensor | Ejecuta la mitigacion sobre un recurso bajo ataque. |
| `STATUS` | Cliente -> Servidor | Ambos | Consulta el estado actual del juego. |
| `QUIT` | Cliente -> Servidor | Ambos | El jugador abandona la partida. |
| `NOTIFY` | Servidor -> Cliente | - | El servidor notifica un evento importante al cliente. |

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

- `sala_id`: ID numerico de la sala a la que se quiere unir. Usar `0` para crear una nueva sala.
- `username`: Nombre del jugador (maximo 20 caracteres, sin espacios).

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

No lleva parametros. El servidor responde indicando si hay un recurso critico en la casilla actual.

---

#### ATTACK

```text
ATTACK <recurso_id>
```

- `recurso_id`: ID del recurso critico que se va a atacar (obtenido al hacer SCAN exitoso).

**Ejemplo:**

```text
ATTACK 2
```

---

#### DEFEND

```text
DEFEND <recurso_id>
```

- `recurso_id`: ID del recurso critico que se va a defender.

**Ejemplo:**

```text
DEFEND 2
```

---

#### STATUS

```text
STATUS
```

El servidor responde con la posicion actual del jugador, su rol y el estado general de la partida.

---

#### QUIT

```text
QUIT
```

El jugador notifica al servidor que abandona la partida. El servidor limpia su sesion.

### 3.2 Mensajes del Servidor al Cliente

Todas las respuestas del servidor siguen este formato:

```text
<codigo> <mensaje>
```

| Codigo | Significado |
|---|---|
| `200` | OK - Accion ejecutada correctamente. |
| `201` | FOUND - Se encontro un recurso critico. |
| `400` | BAD_REQUEST - Mensaje mal formado. |
| `403` | FORBIDDEN - Accion no permitida para ese rol. |
| `404` | NOT_FOUND - Recurso o sala no encontrada. |
| `409` | CONFLICT - Accion invalida en el estado actual. |
| `500` | SERVER_ERROR - Error interno del servidor. |

**Ejemplos de respuestas:**

```text
200 OK MOVE_ACCEPTED POS:5,7
201 FOUND RESOURCE_ID:2 POS:5,7
403 FORBIDDEN ACTION_NOT_ALLOWED_FOR_ROLE
400 BAD_REQUEST UNKNOWN_COMMAND
```

---

#### NOTIFY (Servidor -> Cliente, sin solicitud previa)

El servidor puede enviar notificaciones en cualquier momento:

```text
NOTIFY <tipo_evento> <datos>
```

| Tipo de evento | Cuando se envia |
|---|---|
| `ATTACK_STARTED` | A todos los defensores cuando un atacante lanza un ataque. |
| `ATTACK_MITIGATED` | A todos cuando un defensor mitiga el ataque a tiempo. |
| `GAME_OVER` | A todos cuando la partida termina. |
| `PLAYER_JOINED` | A todos cuando un nuevo jugador entra a la sala. |
| `PLAYER_LEFT` | A todos cuando un jugador abandona la sala. |

**Ejemplos:**

```text
NOTIFY ATTACK_STARTED RESOURCE_ID:1 ATTACKER:hacker99
NOTIFY ATTACK_MITIGATED RESOURCE_ID:1 DEFENDER:defender01
NOTIFY GAME_OVER RESULT:ATTACKERS_WIN
NOTIFY GAME_OVER RESULT:DEFENDERS_WIN
```

---

## 4. Reglas de Procedimiento

### 4.1 Flujo general de una partida

1. El cliente se conecta al servidor por TCP.
2. El cliente envia `JOIN` para unirse o crear una sala.
3. El servidor asigna el rol (Atacante o Defensor) segun la base de datos.
4. La partida inicia cuando hay al menos 1 atacante y 1 defensor.
5. Los jugadores interactuan enviando comandos segun su rol.
6. La partida termina cuando:
	a. Un atacante ataca un recurso y no hay defensa a tiempo -> ganan atacantes.  
	b. Todos los ataques son mitigados -> ganan defensores.

### 4.2 Restricciones por rol

- Solo el **Atacante** puede usar: `SCAN`, `ATTACK`.
- Solo el **Defensor** puede usar: `DEFEND`.
- Ambos pueden usar: `MOVE`, `STATUS`, `QUIT`.
- Cualquier intento de usar un comando del rol contrario retorna `403 FORBIDDEN`.

### 4.3 Restricciones del mapa

- El plano es una cuadricula de **20x20** (posiciones de 0,0 a 19,19).
- Si un jugador intenta moverse fuera del limite, el servidor retorna `409 CONFLICT`.
- Los recursos criticos tienen posicion fija al inicio de cada partida.

### 4.4 Manejo de errores y excepciones

| Situacion | Comportamiento del servidor |
|---|---|
| Mensaje con formato incorrecto | Responde `400 BAD_REQUEST` y mantiene la conexion. |
| Comando desconocido | Responde `400 BAD_REQUEST UNKNOWN_COMMAND`. |
| Cliente se desconecta abruptamente | El servidor elimina al jugador de la sala y notifica. |
| Sala no existe | Responde `404 NOT_FOUND`. |
| Fallo en resolucion DNS | El cliente maneja la excepcion sin cerrar la aplicacion. |
| Error interno del servidor | Responde `500 SERVER_ERROR` y loguea el evento. |

---

## 5. Ejemplos de Flujo

### Ejemplo 1: Atacante encuentra y ataca un recurso

```text
Cliente (Atacante)                         Servidor
----------------------------------------
JOIN 0 hacker99                      ->
<- 200 OK JOINED ROOM:1 ROLE:ATTACKER POS:0,0
MOVE RIGHT                           ->
<- 200 OK MOVE_ACCEPTED POS:1,0
SCAN                                 ->
<- 201 FOUND RESOURCE_ID:2 POS:1,0
ATTACK 2                             ->
<- 200 OK ATTACK_LAUNCHED RESOURCE_ID:2
<- [A defensores] NOTIFY ATTACK_STARTED RESOURCE_ID:2 ATTACKER:hacker99
```

### Ejemplo 2: Defensor mitiga el ataque

```text
Cliente (Defensor)                         Servidor
----------------------------------------
<- NOTIFY ATTACK_STARTED RESOURCE_ID:2 ATTACKER:hacker99
MOVE DOWN                            ->
<- 200 OK MOVE_ACCEPTED POS:1,1
DEFEND 2                             ->
<- 200 OK DEFENSE_SUCCESS RESOURCE_ID:2
<- [A todos] NOTIFY ATTACK_MITIGATED RESOURCE_ID:2 DEFENDER:defender01
```

### Ejemplo 3: Mensaje mal formado

```text
Cliente                                   Servidor
----------------------------------------
ATACAR servidor1                     ->
<- 400 BAD_REQUEST UNKNOWN_COMMAND
```

---

## 6. Logging

El servidor debe registrar cada peticion y respuesta con el siguiente formato:

```text
[TIMESTAMP] [IP:PUERTO] REQUEST: <mensaje_recibido>
[TIMESTAMP] [IP:PUERTO] RESPONSE: <mensaje_enviado>
```

**Ejemplo:**

```text
[2026-03-13 19:05:32] [192.168.56.101:54231] REQUEST: MOVE RIGHT
[2026-03-13 19:05:32] [192.168.56.101:54231] RESPONSE: 200 OK MOVE_ACCEPTED POS:6,3
```
