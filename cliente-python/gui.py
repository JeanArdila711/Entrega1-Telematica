import sys
import tkinter as tk
from tkinter import messagebox, simpledialog
from client import GCPClient, SERVER_PORT

CELL_SIZE = 28
MAP_SIZE  = 20
WINDOW_TITLE = "GCP Client - Juego Multijugador"

BG        = '#0f0f14'   # fondo ventana
GRID_BG   = '#0f0f14'   # fondo celdas
GRID_LINE = '#282838'   # líneas cuadrícula
PANEL_BG  = '#1e1e1e'   # paneles laterales/inferiores
LOG_BG    = '#141414'
LOG_FG    = '#b4ffb4'
BTN_BG    = '#2a2a2a'
STATUS_FG = '#cccccc'

class GameGUI:
    def __init__(self, root, host, port):
        self.root = root
        self.root.title(WINDOW_TITLE)
        self.root.resizable(True, True)
        self.root.configure(bg=PANEL_BG)

        self.client = GCPClient(on_message_callback=self.handle_server_message)
        self.resource_positions = []

        self._build_ui()
        self._connect(host, port)

    def _build_ui(self):
        # ── Barra de estado superior ───────────────────
        top = tk.Frame(self.root, bg=PANEL_BG, pady=4)
        top.pack(side=tk.TOP, fill=tk.X)

        self.lbl_status = tk.Label(top, text="Desconectado",
                                   fg=STATUS_FG, bg=PANEL_BG,
                                   font=('Courier', 10))
        self.lbl_status.pack(side=tk.LEFT, padx=8)

        self.lbl_pos  = tk.Label(top, text="Pos: -",
                                 fg=STATUS_FG, bg=PANEL_BG, font=('Courier', 10))
        self.lbl_pos.pack(side=tk.LEFT, padx=8)

        self.lbl_role = tk.Label(top, text="Rol: -",
                                 fg=STATUS_FG, bg=PANEL_BG, font=('Courier', 10))
        self.lbl_role.pack(side=tk.LEFT, padx=8)

        tk.Frame(self.root, bg='#333', height=1).pack(fill=tk.X)

        # ── Zona central: mapa + panel derecho ────────
        center = tk.Frame(self.root, bg=PANEL_BG)
        center.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # Mapa
        canvas_px = CELL_SIZE * MAP_SIZE
        self.canvas = tk.Canvas(center, width=canvas_px, height=canvas_px,
                                bg=GRID_BG, highlightthickness=0)
        self.canvas.pack(side=tk.LEFT, padx=8, pady=8)
        self._draw_grid()

        # Panel derecho: controles + log
        right = tk.Frame(center, bg=PANEL_BG, padx=5, pady=5)
        right.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Sección controles
        ctrl = tk.LabelFrame(right, text="Controles", fg='#aaa',
                             bg=PANEL_BG, font=('Courier', 9),
                             bd=1, relief=tk.GROOVE)
        ctrl.pack(fill=tk.X, pady=(0, 6))

        # Cruceta de movimiento
        move = tk.Frame(ctrl, bg=PANEL_BG)
        move.pack(pady=6)

        tk.Button(move, text='^', width=3,
                  command=lambda: self.client.move('UP'),
                  bg=BTN_BG, fg='white', relief=tk.FLAT,
                  activebackground='#555').grid(row=0, column=1, pady=1, padx=1)
        tk.Button(move, text='<', width=3,
                  command=lambda: self.client.move('LEFT'),
                  bg=BTN_BG, fg='white', relief=tk.FLAT,
                  activebackground='#555').grid(row=1, column=0, pady=1, padx=1)
        tk.Button(move, text='v', width=3,
                  command=lambda: self.client.move('DOWN'),
                  bg=BTN_BG, fg='white', relief=tk.FLAT,
                  activebackground='#555').grid(row=1, column=1, pady=1, padx=1)
        tk.Button(move, text='>', width=3,
                  command=lambda: self.client.move('RIGHT'),
                  bg=BTN_BG, fg='white', relief=tk.FLAT,
                  activebackground='#555').grid(row=1, column=2, pady=1, padx=1)

        tk.Frame(ctrl, bg='#333', height=1).pack(fill=tk.X, padx=6)

        # Botones de acción
        actions = tk.Frame(ctrl, bg=PANEL_BG)
        actions.pack(fill=tk.X, padx=6, pady=6)

        btn_cfg = [
            ('SCAN',   '#1a3a6a', self.client.scan),
            ('ATTACK', '#6a1a1a', self._do_attack),
            ('DEFEND', '#1a5a1a', self._do_defend),
            ('STATUS', '#3a3a3a', self.client.status),
            ('QUIT',   '#4a2a00', self.client.quit_game),
        ]
        for text, color, cmd in btn_cfg:
            tk.Button(actions, text=text, width=12, command=cmd,
                      bg=color, fg='white', relief=tk.FLAT,
                      activebackground='#555',
                      font=('Courier', 9)).pack(fill=tk.X, pady=2)

        # Sección log
        log_frame = tk.LabelFrame(right, text="Log de mensajes GCP", fg='#aaa',
                                  bg=PANEL_BG, font=('Courier', 9),
                                  bd=1, relief=tk.GROOVE)
        log_frame.pack(fill=tk.BOTH, expand=True)

        self.log_box = tk.Text(log_frame, bg=LOG_BG, fg=LOG_FG,
                               font=('Courier', 9), state=tk.DISABLED,
                               wrap=tk.WORD, relief=tk.FLAT)
        scrollbar = tk.Scrollbar(log_frame, command=self.log_box.yview,
                                 bg=PANEL_BG)
        self.log_box.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.log_box.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # Teclas de movimiento
        self.root.bind('<Up>',    lambda e: self.client.move('UP'))
        self.root.bind('<Down>',  lambda e: self.client.move('DOWN'))
        self.root.bind('<Left>',  lambda e: self.client.move('LEFT'))
        self.root.bind('<Right>', lambda e: self.client.move('RIGHT'))

    def _draw_grid(self):
        for i in range(MAP_SIZE):
            for j in range(MAP_SIZE):
                x1 = j * CELL_SIZE
                y1 = i * CELL_SIZE
                self.canvas.create_rectangle(
                    x1, y1, x1 + CELL_SIZE, y1 + CELL_SIZE,
                    outline=GRID_LINE, fill=GRID_BG)

    def _update_map(self):
        self._draw_grid()

        # Unir recursos conocidos: los que manda el servidor al defensor
        # (self.client.resources) + los que el atacante descubre con SCAN
        # (self.resource_positions)
        recursos_a_dibujar = list(self.client.resources) + [
            r for r in self.resource_positions if r not in self.client.resources
        ]

        # Recursos en naranja con letra S (igual que Java)
        for rx, ry in recursos_a_dibujar:
            x1 = rx * CELL_SIZE + 4
            y1 = ry * CELL_SIZE + 4
            x2 = x1 + CELL_SIZE - 8
            y2 = y1 + CELL_SIZE - 8
            self.canvas.create_rectangle(x1, y1, x2, y2,
                                         fill='#ffc800', outline='')
            self.canvas.create_text(x1 + (x2 - x1) // 2, y1 + (y2 - y1) // 2,
                                    text='S', fill='black',
                                    font=('Courier', 8, 'bold'))

        # Jugador: rojo ATTACKER, azul DEFENDER
        if self.client.username:
            px = self.client.x * CELL_SIZE + 3
            py = self.client.y * CELL_SIZE + 3
            sz = CELL_SIZE - 6
            color = '#dc3c3c' if self.client.role == 'ATTACKER' else '#3c82dc'
            self.canvas.create_oval(px, py, px + sz, py + sz,
                                    fill=color, outline='')
            inicial = self.client.role[0] if self.client.role else '?'
            self.canvas.create_text(px + sz // 2, py + sz // 2,
                                    text=inicial, fill='white',
                                    font=('Courier', 9, 'bold'))

    def _connect(self, host, port):
        username = simpledialog.askstring("Usuario", "Tu nombre de usuario:")
        if not username:
            username = "jugador1"

        room = simpledialog.askstring("Sala", "ID de sala (0 para crear nueva):", initialvalue="0")
        if not room:
            room = "0"

        if self.client.connect(host, port):
            self.client.join(int(room), username)
            self.log("Conectado al servidor.")
        else:
            messagebox.showerror("Error", "No se pudo conectar al servidor.")

    def handle_server_message(self, code, message):
        self.root.after(0, self._process_message, code, message)

    def _process_message(self, code, message):
        self.log(f"<- {message}")

        # Mostrar ID de sala de forma visible cuando se une/crea una sala
        if code == '200' and 'JOINED' in message:
            for part in message.split():
                if part.startswith('ROOM:'):
                    room_id = part.split(':')[1]
                    self.log(f"*** ID DE SALA: {room_id} ***")
                    self.log(f"*** Comparte este ID con el otro jugador ***")
                    messagebox.showinfo(
                        "Sala asignada",
                        f"Estás en la sala #{room_id}\n\n"
                        f"Dile al otro jugador que use este ID para unirse."
                    )
                    break

        # Actualizar barra de estado
        if self.client.role and self.client.username:
            color = '#dc3c3c' if self.client.role == 'ATTACKER' else '#3c82dc'
            self.lbl_status.config(text=f"Conectado  |  {self.client.username}",
                                   fg='#00b400')
            self.lbl_pos.config(text=f"Pos: ({self.client.x}, {self.client.y})")
            self.lbl_role.config(text=f"Rol: {self.client.role}", fg=color)

        self._update_map()

        if 'NOTIFY' in message:
            if 'ATTACK_STARTED' in message:
                # Extraer TIMEOUT si está presente
                timeout = None
                for part in message.split():
                    if part.startswith('TIMEOUT:'):
                        timeout = part.split(':')[1]
                        break
                if timeout:
                    messagebox.showwarning(
                        "ATAQUE",
                        f"Recurso bajo ataque!\n"
                        f"Los defensores tienen {timeout} segundos para mitigar.\n\n{message}"
                    )
                else:
                    messagebox.showwarning("ATAQUE", f"Recurso bajo ataque!\n{message}")
            elif 'RESOURCE_DOWN' in message:
                messagebox.showerror(
                    "RECURSO COMPROMETIDO",
                    f"Un recurso cayo: se acabo el tiempo para defenderlo.\n{message}"
                )
            elif 'GAME_OVER' in message:
                messagebox.showinfo("FIN DEL JUEGO", message)
            elif 'ATTACK_MITIGATED' in message:
                messagebox.showinfo("ATAQUE MITIGADO", message)

        if '201' in code and 'FOUND' in message:
            for part in message.split():
                if part.startswith('POS:'):
                    coords = part.split(':')[1].split(',')
                    pos = (int(coords[0]), int(coords[1]))
                    if pos not in self.resource_positions:
                        self.resource_positions.append(pos)

        if code == 'DISCONNECTED':
            self.lbl_status.config(text="Desconectado", fg='red')
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
        self.log_box.config(state=tk.NORMAL)
        self.log_box.insert(tk.END, message + '\n')
        self.log_box.see(tk.END)
        self.log_box.config(state=tk.DISABLED)


# ── Punto de entrada ──────────────────────────────────
if __name__ == '__main__':
    host = sys.argv[1] if len(sys.argv) > 1 else "localhost"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else SERVER_PORT

    root = tk.Tk()
    app = GameGUI(root, host, port)
    root.mainloop()
