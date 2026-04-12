import socket
import threading

SERVER_PORT = 8080
BUFFER_SIZE = 1024

class GCPClient:
    def __init__(self, on_message_callback):
        self.sock = None
        self.connected = False
        self.username = None
        self.role = None
        self.x = 0
        self.y = 0
        self.room_id = None
        # Posiciones de recursos criticos (solo las conoce el defensor)
        self.resources = []
        # Función que se llama cada vez que llega un mensaje del servidor
        self.on_message = on_message_callback

    def connect(self, host, port=SERVER_PORT):
        """Conecta al servidor resolviendo el hostname (sin IPs hardcodeadas)."""
        # Resolución DNS explícita — si falla, se avisa pero no se cierra la app
        try:
            results = socket.getaddrinfo(host, port, socket.AF_INET, socket.SOCK_STREAM)
            if not results:
                print(f"Error: no se pudo resolver el hostname '{host}'")
                return False
            family, socktype, proto, canonname, sockaddr = results[0]
            print(f"Hostname '{host}' resuelto a {sockaddr[0]}")
        except socket.gaierror as e:
            print(f"Error de resolución DNS para '{host}': {e}")
            return False

        try:
            self.sock = socket.socket(family, socktype, proto)
            self.sock.connect(sockaddr)
            self.connected = True

            # Hilo que escucha mensajes del servidor en segundo plano
            listener = threading.Thread(target=self._listen, daemon=True)
            listener.start()
            return True
        except Exception as e:
            print(f"Error conectando: {e}")
            return False

    def _listen(self):
        """Escucha mensajes del servidor constantemente (corre en hilo aparte)"""
        while self.connected:
            try:
                data = self.sock.recv(BUFFER_SIZE)
                if not data:
                    self.connected = False
                    self.on_message("DISCONNECTED", "")
                    break

                message = data.decode('utf-8').strip()

                # Puede llegar más de un mensaje junto
                for line in message.split('\n'):
                    line = line.strip()
                    if line:
                        self._parse_server_message(line)

            except Exception as e:
                if self.connected:
                    print(f"Error recibiendo: {e}")
                self.connected = False
                break

    def _parse_server_message(self, message):
        """Procesa cada mensaje del servidor y actualiza el estado del cliente"""
        parts = message.split()

        # Respuesta al JOIN — actualiza rol y posición inicial
        if len(parts) >= 6 and parts[0] == '200' and 'JOINED' in parts:
            for part in parts:
                if part.startswith('ROLE:'):
                    self.role = part.split(':')[1]
                if part.startswith('POS:'):
                    coords = part.split(':')[1].split(',')
                    self.x = int(coords[0])
                    self.y = int(coords[1])
                if part.startswith('ROOM:'):
                    self.room_id = int(part.split(':')[1])
                # Defensor recibe posiciones de recursos: RESOURCES:x1,y1;x2,y2
                if part.startswith('RESOURCES:'):
                    self.resources = []
                    res_str = part.split(':', 1)[1]
                    for pair in res_str.split(';'):
                        xy = pair.split(',')
                        if len(xy) == 2:
                            try:
                                self.resources.append((int(xy[0]), int(xy[1])))
                            except ValueError:
                                pass

        # Respuesta al MOVE — actualiza posición
        elif len(parts) >= 4 and parts[0] == '200' and 'MOVE_ACCEPTED' in parts:
            for part in parts:
                if part.startswith('POS:'):
                    coords = part.split(':')[1].split(',')
                    self.x = int(coords[0])
                    self.y = int(coords[1])

        # Notificar a la GUI con el mensaje completo
        self.on_message(parts[0], message)

    def send_command(self, command):
        """Envía un comando al servidor"""
        if not self.connected:
            print("No conectado al servidor")
            return
        try:
            self.sock.send((command + '\n').encode('utf-8'))
        except Exception as e:
            print(f"Error enviando comando: {e}")
            self.connected = False

    # ── Comandos del protocolo ────────────────────────

    def join(self, room_id, username):
        self.username = username
        self.send_command(f"JOIN {room_id} {username}")

    def move(self, direction):
        self.send_command(f"MOVE {direction}")

    def scan(self):
        self.send_command("SCAN")

    def attack(self, resource_id):
        self.send_command(f"ATTACK {resource_id}")

    def defend(self, resource_id):
        self.send_command(f"DEFEND {resource_id}")

    def status(self):
        self.send_command("STATUS")

    def quit_game(self):
        self.send_command("QUIT")
        self.connected = False
        if self.sock:
            self.sock.close()
