"""
Servidor de identidad para GCP (Game Communication Protocol)
Telematica 2026-1

Corre en el puerto 9090 y responde consultas del servidor de juego.

Protocolo interno:
  Peticion:  GETUSER <username>\n
  Respuesta: 200 OK ROLE:<rol>\n        (si el usuario existe)
             404 NOT_FOUND\n            (si no existe)

Ejecutar: python auth_server.py
"""

import socket

PORT = 9090
USERS_FILE = "users.txt"


def load_users():
    """Lee el archivo users.txt y retorna un diccionario {username: role}."""
    users = {}
    try:
        with open(USERS_FILE, "r") as f:
            for line in f:
                line = line.strip()
                # Ignorar líneas vacías y comentarios
                if not line or line.startswith("#"):
                    continue
                parts = line.split(":")
                if len(parts) == 3:
                    username, password, role = parts
                    users[username.strip()] = {
                        "password": password.strip(),
                        "role": role.strip()
                    }
    except FileNotFoundError:
        print(f"ERROR: No se encontró el archivo {USERS_FILE}")
    return users


def handle_client(conn, addr, users):
    """Atiende una conexión del servidor de juego."""
    print(f"[AUTH] Consulta de {addr[0]}:{addr[1]}")
    try:
        data = conn.recv(256).decode().strip()
        print(f"[AUTH] Recibido: {data}")

        # Esperamos: GETUSER <username> <password>
        parts = data.split()
        if len(parts) == 3 and parts[0] == "GETUSER":
            username = parts[1]
            password = parts[2]
            if username in users and users[username]["password"] == password:
                role = users[username]["role"]
                response = f"200 OK ROLE:{role}\n"
            elif username in users:
                response = "403 FORBIDDEN WRONG_PASSWORD\n"
            else:
                response = "404 NOT_FOUND\n"
        else:
            response = "400 BAD_REQUEST\n"

        print(f"[AUTH] Enviando: {response.strip()}")
        conn.sendall(response.encode())

    except Exception as e:
        print(f"[AUTH] Error: {e}")
    finally:
        conn.close()


def main():
    # Cargar usuarios al iniciar
    users = load_users()
    print(f"[AUTH] {len(users)} usuario(s) cargado(s): {list(users.keys())}")

    # Crear socket TCP
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind(("0.0.0.0", PORT))
    server_sock.listen(5)
    print(f"[AUTH] Servidor de identidad corriendo en el puerto {PORT}")

    while True:
        conn, addr = server_sock.accept()
        handle_client(conn, addr, users)


if __name__ == "__main__":
    main()
