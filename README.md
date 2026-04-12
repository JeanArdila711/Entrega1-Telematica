# GCP - Game Communication Protocol

Proyecto de Internet: Arquitectura y Protocolos — 2026-1  
**Autores:** Jean Carlo Ardila Acevedo - Valentina Zapata Acosta

---

## Que es esto?

Un juego multijugador en red donde un jugador asume el rol de **Atacante** y otro de **Defensor**
dentro de un simulador de centro de datos. El atacante explora un mapa buscando servidores criticos
para atacarlos, y el defensor debe mitigarlos antes de que todos caigan.

El sistema esta compuesto por tres componentes:

| Componente | Lenguaje | Puerto |
|---|---|---|
| Servidor de juego | C | 8080 |
| Servidor HTTP (monitor) | C | 8081 |
| Servidor de identidad | Python | 9090 |
| Cliente | Java o Python | - |

---

## Estructura del repositorio

```
Entrega1-Telematica/
├── server/
│   ├── server.c        # Servidor principal: sockets, hilos, protocolo GCP
│   ├── game.c          # Logica del juego: mover, atacar, defender
│   ├── game.h          # Definiciones de estructuras y funciones
│   ├── http_server.c   # Servidor HTTP: login web y monitor de partidas
│   └── Makefile
├── auth-server/
│   ├── auth_server.py  # Servidor de identidad
│   └── users.txt       # Base de datos de usuarios (usuario:password:rol)
├── cliente-java/
│   └── Client.java     # Cliente con interfaz grafica en Java Swing
├── cliente-python/
│   ├── gui.py          # Interfaz grafica en tkinter
│   └── client.py       # Logica de conexion y protocolo
└── protocolo/
    └── protocolo.md    # Especificacion completa del protocolo GCP
```

---

## Como ejecutar

### Requisitos

- Linux o WSL (para compilar el servidor en C)
- `gcc` y `make`
- `python3`
- `java` y `javac`

### Paso 1 — Compilar el servidor

```bash
cd server/
make
```

### Paso 2 — Arrancar el servidor de identidad

En una terminal aparte:

```bash
cd auth-server/
python3 auth_server.py
```

### Paso 3 — Arrancar el servidor de juego

En otra terminal:

```bash
cd server/
mkdir -p logs
./server 8080 logs/server.log
```

Si el servidor de identidad esta en otra maquina:

```bash
./server 8080 logs/server.log <hostname-del-auth-server>
```

### Paso 4 — Abrir la interfaz web

En el navegador:

```
http://<IP-del-servidor>:8081
```

Ahi puedes hacer login y ver las partidas activas.

### Paso 5 — Conectar un cliente

**Java:**
```bash
cd cliente-java/
javac Client.java
java Client <IP-del-servidor> 8080
```

**Python:**
```bash
cd cliente-python/
python3 gui.py <IP-del-servidor> 8080
```

---

## Usuarios de prueba

| Usuario | Contrasena | Rol |
|---|---|---|
| alice | 1234 | ATTACKER |
| bob | 5678 | DEFENDER |
| carlos | abcd | ATTACKER |
| diana | pass | DEFENDER |

---

## Como se juega

1. Conecta dos clientes: uno con un usuario ATTACKER y otro con un DEFENDER.
2. La partida inicia automaticamente cuando hay al menos uno de cada rol en la misma sala.
3. El **ATTACKER** se mueve por el mapa con las flechas y usa SCAN para detectar recursos criticos.
4. Cuando encuentra un recurso, usa ATTACK para atacarlo.
5. El **DEFENDER** recibe una notificacion de ataque y debe usar DEFEND para mitigarlo.
6. Ganan los defensores si mitigan todos los ataques. Ganan los atacantes si todos los recursos quedan bajo ataque al mismo tiempo.

---

## Protocolo

Ver la especificacion completa en [`protocolo/protocolo.md`](protocolo/protocolo.md).

---

## Despliegue en AWS

El servidor se desplego en una instancia EC2 de AWS Educate. Estos son los pasos:

### 1. Crear la instancia EC2

- AMI: **Ubuntu Server 22.04 LTS**
- Tipo: **t2.micro** (free tier)
- Key pair: crear una nueva (`.pem`) y guardarla bien
- Storage: 8 GB por defecto es suficiente

### 2. Configurar Security Group

Abrir los siguientes puertos **inbound** (entrada) desde `0.0.0.0/0`:

| Puerto | Protocolo | Uso |
|---|---|---|
| 22 | TCP | SSH para administrar la instancia |
| 8080 | TCP | Servidor de juego GCP |
| 8081 | TCP | Servidor HTTP (interfaz web) |
| 9090 | TCP | Servidor de identidad (auth) |

### 3. Conectarse por SSH

```bash
chmod 400 mi-llave.pem
ssh -i mi-llave.pem ubuntu@<DNS-publico-de-la-instancia>
```

### 4. Instalar dependencias

```bash
sudo apt update
sudo apt install -y gcc make python3 git
```

### 5. Clonar el repo y compilar

```bash
git clone <url-del-repo>
cd Entrega1-Telematica/server
make
```

### 6. Arrancar los servicios

En dos terminales SSH separadas (o usando `tmux` / `screen`):

**Terminal 1 — servidor de identidad:**
```bash
cd ~/Entrega1-Telematica/auth-server
python3 auth_server.py
```

**Terminal 2 — servidor de juego:**
```bash
cd ~/Entrega1-Telematica/server
./server 8080 logs/server.log localhost
```

### 7. Probar desde un cliente local

Desde tu maquina (no la EC2), obten el DNS publico de la instancia (por ejemplo
`ec2-54-123-45-67.compute-1.amazonaws.com`) y conecta el cliente:

```bash
# Java
java Client ec2-54-123-45-67.compute-1.amazonaws.com 8080

# Python
python3 gui.py ec2-54-123-45-67.compute-1.amazonaws.com 8080
```

La interfaz web queda en:

```
http://ec2-54-123-45-67.compute-1.amazonaws.com:8081
```

### Notas

- El codigo no tiene IPs hardcodeadas: usa `getaddrinfo` (C) y `InetAddress.getByName` (Java)
  para resolver el hostname de AWS dinamicamente.
- Si la instancia se reinicia, el DNS publico cambia. Para un DNS fijo se puede asociar una
  Elastic IP a la instancia.
- Los logs quedan en `server/logs/server.log` dentro de la EC2.
