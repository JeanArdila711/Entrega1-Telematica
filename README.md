# GCP - Game Communication Protocol

![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white)
![Python](https://img.shields.io/badge/python-3670A0?style=for-the-badge&logo=python&logoColor=ffdd54)
![Java](https://img.shields.io/badge/java-%23ED8B00.svg?style=for-the-badge&logo=openjdk&logoColor=white)
![AWS deployed](https://img.shields.io/badge/AWS-Deployed-FF9900?style=for-the-badge&logo=amazon-aws&logoColor=white)

**Proyecto de Internet: Arquitectura y Protocolos — 2026-1**  
**Autores:** Jean Carlo Ardila Acevedo - Valentina Zapata Acosta

---

## Acerca del Proyecto

Un juego interactivo multijugador en red donde un jugador asume el rol de **Atacante** y otro de **Defensor** dentro de un simulador de centro de datos. 

El modelo de juego consiste en que el atacante explora un mapa buscando servidores críticos para vulnerarlos, mientras que el defensor es notificado y debe mitigar los ataques antes de que todos los servidores caigan.

> **Nota de Despliegue:** El back-end de este proyecto (servidores de juego e identidad) se encuentra **completamente desplegado y operativo en una instancia de Amazon EC2 (AWS)**. Consulta la sección de [Despliegue en AWS](#-despliegue-en-aws) para más detalles.

---

## Arquitectura del Sistema

El ecosistema de la aplicación adopta un modelo distribuido Cliente-Servidor compuesto por los siguientes componentes principales:

| Componente | Lenguaje | Puerto | Propósito |
|---|---|---|---|
| **Servidor de Juego** | C | `8080` | Manejo de sockets, hilos y lógica core vía protocolo GCP. |
| **Servidor HTTP (Monitor)** | C | `8081` | Servidor web embebido para monitoreo de partidas activas. |
| **Servidor de Identidad** | Python | `9090` | Validación y gestión de usuarios/roles. |
| **Clientes** | Java / Python | N/A | Interfaces gráficas (Swing/Tkinter) de interacción para los jugadores. |

---

## Estructura del Repositorio

```text
Entrega1-Telematica/
├── server/
│   ├── server.c        # Servidor principal: sockets, hilos, protocolo GCP
│   ├── game.c          # Lógica del juego: mover, atacar, defender
│   ├── game.h          # Definiciones de estructuras y funciones
│   ├── http_server.c   # Servidor HTTP: login web y monitor de partidas
│   └── Makefile
├── auth-server/
│   ├── auth_server.py  # Servidor de identidad
│   └── users.txt       # Base de datos de usuarios (usuario:password:rol)
├── cliente-java/
│   └── Client.java     # Cliente con interfaz gráfica en Java Swing
├── cliente-python/
│   ├── gui.py          # Interfaz gráfica en tkinter
│   └── client.py       # Lógica de conexión y protocolo
└── protocolo/
    └── protocolo.md    # Especificación completa del protocolo GCP
```

---

## Guía de Juego

1. Conecta dos clientes independientes: uno ingresando con rol `ATTACKER` y otro como `DEFENDER`.
2. La partida se iniciará automáticamente cuando haya al menos un responsable de cada rol dentro de la sala.
3. El **ATTACKER** navega por el mapa empleando los controles de dirección y utiliza la acción `SCAN` para ubicar nodos críticos ocultos.
4. Al encontrar un recurso vulnerable, el atacante despliega la acción `ATTACK`.
5. El **DEFENDER** recibe alertas inmediatas del ataque en curso y debe apresurarse a utilizar el comando `DEFEND` sobre el nodo afectado.
6. **Condiciones de Victoria:** El equipo defensor gana si logra mitigar y asegurar la totalidad de los ataques. El equipo atacante gana si consigue saturar y colocar bajo ataque todos los recursos críticos de forma simultánea.

---

## Cuentas de Prueba Disponibles

| Usuario | Contraseña | Rol |
|---|---|---|
| `alice` | `1234` | **ATTACKER** |
| `bob` | `5678` | **DEFENDER** |
| `carlos` | `abcd` | **ATTACKER** |
| `diana` | `pass` | **DEFENDER** |

---

## Configuración y Ejecución Local

Si deseas ejecutar o compilar el proyecto en modo local, sigue estos pasos:

### 1. Requisitos Previos
* Linux o Windows Subsystem for Linux (WSL)
* Utilidades de compilación C: `gcc`, `make`
* Motores de ejecución: `python3`, `java`, `javac`

### 2. Puesta en Marcha (Servidores)

Levanta primero el sistema de autenticación (Terminal 1):
```bash
cd auth-server/
python3 auth_server.py
```

Compila y lanza el motor del juego (Terminal 2):
```bash
cd server/
make
mkdir -p logs
./server 8080 logs/server.log   # Añade el hostname al final si auth-server se ejecuta remotamente
```

### 3. Conexión de Clientes

**Cliente Java:**
```bash
cd cliente-java/
javac Client.java
java Client <IP-del-servidor> 8080
```

**Cliente Python:**
```bash
cd cliente-python/
python3 gui.py <IP-del-servidor> 8080
```

**Monitor Interfaz Web:** Abre en tu navegador `http://<IP-del-servidor>:8081`

---

## Despliegue en AWS

Actualmente el sistema está completamente desplegado para su acceso público sobre infraestructura **AWS Educate (EC2)**. 

### Infraestructura AWS
- **AMI:** Ubuntu Server 24.04 LTS
- **Instancia:** `t2.micro`
- **Security Groups (Inbound):** Puertos `22` (SSH), `8080` (GCP), `8081` (HTTP Monitor), `9090` (Auth).

### Conexión como Cliente a AWS

Desde una máquina local (sin necesidad de configurar servidores de desarrollo), se debe utilizar el DNS público dinámico proveído por la instancia EC2 `(ej. ec2-n-n-n...amazonaws.com)` a través del servicio DNS incorporado en el proyecto. 

```bash
# Ejemplo en Java:
java Client <DNS-publico-de-la-instancia> 8080

# Ejemplo en Python:
python3 gui.py <DNS-publico-de-la-instancia> 8080
```
*(Puedes ingresar al panel web administrativo empleando dicho DNS apuntando al puerto `8081` en el navegador: `http://<DNS-publico-de-la-instancia>:8081`)*

### Configuración para el Administrador
Si se requiere provisionar de cero el ambiente de AWS:

1. Ingresar vía SSH: `ssh -i mi-llave.pem ubuntu@<DNS-publico-de-la-instancia>`
2. Instalar el stack de dependencias (`sudo apt update && sudo apt install -y gcc make python3 git`).
3. Clonar el repositorio y efectuar `make` en el directorio `Entrega1-Telematica/server`.
4. Iniciar mediante background jobs (`tmux`/`screen`) tanto el módulo de `auth_server.py` como `./server 8080 logs/server.log localhost` apuntando correctamente los puertos localmente.

---

## Documentación del Protocolo

Para visualizar el detalle de la mensajería interna, estados y payloads que rigen este servicio, dirígete a la especificación formal del modelo en: [`protocolo/protocolo.md`](protocolo/protocolo.md).
