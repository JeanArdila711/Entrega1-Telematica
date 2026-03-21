import tkinter as tk
from tkinter import messagebox, simpledialog
from client import GCPClient, SERVER_PORT

CELL_SIZE = 30
MAP_SIZE = 20
WINDOW_TITLE = "GCP - Game Communication Protocol"

class GameGUI:
    def __init__(self, root):
        self.root = root
        self.root.title(WINDOW_TITLE)
        self.root.resizable(False, False)

        self.client = GCPClient(on_message_callback=self.handle_server_message)
        self.resource_positions = []  # Posiciones conocidas de recursos (defensores)

        self._build_ui()
        self._connect()

    def _build_ui(self):
        # ── Panel superior: info del jugador ──────────
        info_frame = tk.Frame(self.root, bg='#1e1e1e', pady=5)
        info_frame.pack(fill=tk.X)

        self.lbl_status = tk.Label(info_frame, text="Conectando...",
                                   fg='white', bg='#1e1e1e', font=('Courier', 11))
        self.lbl_status.pack()

        # ── Mapa ──────────────────────────────────────
        canvas_size = CELL_SIZE * MAP_SIZE
        self.canvas = tk.Canvas(self.root, width=canvas_size, height=canvas_size,
                                bg='#2d2d2d', highlightthickness=0)
        self.canvas.pack(padx=10, pady=5)
        self._draw_grid()

        # ── Log de mensajes ───────────────────────────
        log_frame = tk.Frame(self.root, bg='#1e1e1e')
        log_frame.pack(fill=tk.BOTH, padx=10, pady=5)

        self.log_box = tk.Text(log_frame, height=6, bg='#0d0d0d', fg='#00ff00',
                               font=('Courier', 9), state=tk.DISABLED)
        self.log_box.pack(fill=tk.X)

        # ── Botones de acción ─────────────────────────
        btn_frame = tk.Frame(self.root, bg='#1e1e1e', pady=5)
        btn_frame.pack()

        # Botones de movimiento
        move_frame = tk.Frame(btn_frame, bg='#1e1e1e')
        move_frame.grid(row=0, column=0, padx=20)

        tk.Button(move_frame, text='↑', width=4, command=lambda: self.client.move('UP'),
                  bg='#444', fg='white').grid(row=0, column=1)
        tk.Button(move_frame, text='←', width=4, command=lambda: self.client.move('LEFT'),
                  bg='#444', fg='white').grid(row=1, column=0)
        tk.Button(move_frame, text='↓', width=4, command=lambda: self.client.move('DOWN'),
                  bg='#444', fg='white').grid(row=1, column=1)
        tk.Button(move_frame, text='→', width=4, command=lambda: self.client.move('RIGHT'),
                  bg='#444', fg='white').grid(row=1, column=2)

        # Botones de acción
        action_frame = tk.Frame(btn_frame, bg='#1e1e1e')
        action_frame.grid(row=0, column=1, padx=20)

        tk.Button(action_frame, text='SCAN', width=10,
                  command=self.client.scan,
                  bg='#2255aa', fg='white').pack(pady=2)
        tk.Button(action_frame, text='ATTACK', width=10,
                  command=self._do_attack,
                  bg='#aa2222', fg='white').pack(pady=2)
        tk.Button(action_frame, text='DEFEND', width=10,
                  command=self._do_defend,
                  bg='#22aa22', fg='white').pack(pady=2)
        tk.Button(action_frame, text='STATUS', width=10,
                  command=self.client.status,
                  bg='#555', fg='white').pack(pady=2)

        # Teclas del teclado para moverse
        self.root.bind('<Up>',    lambda e: self.client.move('UP'))
        self.root.bind('<Down>',  lambda e: self.client.move('DOWN'))
        self.root.bind('<Left>',  lambda e: self.client.move('LEFT'))
        self.root.bind('<Right>', lambda e: self.client.move('RIGHT'))

    def _draw_grid(self):
        """Dibuja la cuadrícula del mapa"""
        for i in range(MAP_SIZE):
            for j in range(MAP_SIZE):
                x1 = j * CELL_SIZE
                y1 = i * CELL_SIZE
                x2 = x1 + CELL_SIZE
                y2 = y1 + CELL_SIZE
                self.canvas.create_rectangle(x1, y1, x2, y2,
                                             outline='#444', fill='#2d2d2d')

    def _update_map(self):
        """Redibuja el mapa con la posición actual del jugador"""
        self._draw_grid()

        # Dibujar recursos conocidos (defensores los ven siempre)
        for rx, ry in self.resource_positions:
            x1 = rx * CELL_SIZE + 4
            y1 = ry * CELL_SIZE + 4
            x2 = x1 + CELL_SIZE - 8
            y2 = y1 + CELL_SIZE - 8
            self.canvas.create_rectangle(x1, y1, x2, y2, fill='#ffaa00', outline='')

        # Dibujar jugador
        px = self.client.x * CELL_SIZE + CELL_SIZE // 2
        py = self.client.y * CELL_SIZE + CELL_SIZE // 2
        r = CELL_SIZE // 2 - 4
        color = '#ff4444' if self.client.role == 'ATTACKER' else '#44aaff'
        self.canvas.create_oval(px - r, py - r, px + r, py + r, fill=color, outline='')

        # Etiqueta del jugador
        self.canvas.create_text(px, py, text=self.client.username[0].upper() if self.client.username else '?',
                                fill='white', font=('Courier', 10, 'bold'))

    def _connect(self):
        """Pide datos de conexión y se conecta al servidor"""
        host = simpledialog.askstring("Servidor", "IP del servidor:", initialvalue="localhost")
        if not host:
            host = "localhost"

        username = simpledialog.askstring("Usuario", "Tu nombre de usuario:")
        if not username:
            username = "jugador1"

        room = simpledialog.askstring("Sala", "ID de sala (0 para crear nueva):", initialvalue="0")
        if not room:
            room = "0"

        if self.client.connect(host, SERVER_PORT):
            self.client.join(int(room), username)
            self.log("Conectado al servidor.")
        else:
            messagebox.showerror("Error", "No se pudo conectar al servidor.")

    def handle_server_message(self, code, message):
        """Procesa mensajes del servidor y actualiza la GUI"""
        self.root.after(0, self._process_message, code, message)

    def _process_message(self, code, message):
        self.log(f"← {message}")

        # Actualizar etiqueta de estado
        if self.client.role and self.client.username:
            self.lbl_status.config(
                text=f"👤 {self.client.username}  |  Rol: {self.client.role}  |  "
                     f"Pos: ({self.client.x}, {self.client.y})  |  Sala: {self.client.room_id}"
            )

        # Actualizar mapa en cada respuesta
        self._update_map()

        # Mostrar notificaciones importantes como popup
        if 'NOTIFY' in message:
            if 'ATTACK_STARTED' in message:
                messagebox.showwarning("⚠️ ATAQUE", f"Recurso bajo ataque!\n{message}")
            elif 'GAME_OVER' in message:
                messagebox.showinfo("🏁 FIN DEL JUEGO", message)
            elif 'ATTACK_MITIGATED' in message:
                messagebox.showinfo("✅ ATAQUE MITIGADO", message)

        # Si encontró un recurso, guardarlo en el mapa
        if '201' in code and 'FOUND' in message:
            for part in message.split():
                if part.startswith('POS:'):
                    coords = part.split(':')[1].split(',')
                    pos = (int(coords[0]), int(coords[1]))
                    if pos not in self.resource_positions:
                        self.resource_positions.append(pos)

        if code == 'DISCONNECTED':
            messagebox.showerror("Desconectado", "Se perdió la conexión con el servidor.")

    def _do_attack(self):
        resource_id = simpledialog.askinteger("ATTACK", "ID del recurso a atacar:")
        if resource_id:
            self.client.attack(resource_id)

    def _do_defend(self):
        resource_id = simpledialog.askinteger("DEFEND", "ID del recurso a defender:")
        if resource_id:
            self.client.defend(resource_id)

    def log(self, message):
        """Agrega un mensaje al log de la GUI"""
        self.log_box.config(state=tk.NORMAL)
        self.log_box.insert(tk.END, message + '\n')
        self.log_box.see(tk.END)
        self.log_box.config(state=tk.DISABLED)


# ── Punto de entrada ──────────────────────────────────
if __name__ == '__main__':
    root = tk.Tk()
    app = GameGUI(root)
    root.mainloop()
