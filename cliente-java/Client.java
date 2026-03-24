import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;

/**
 * Cliente Java para el protocolo GCP (Game Communication Protocol)
 * Telematica 2026-1 - Entrega 1
 *
 * Compilar: javac Client.java
 * Ejecutar: java Client <host> <puerto>
 *           java Client game.servidor.com 8080
 */
public class Client extends JFrame {

    // Tamanio del mapa
    static final int GRID_SIZE = 20;
    static final int CELL_SIZE = 28;

    // Conexion
    Socket socket;
    BufferedReader input;
    PrintWriter output;

    // Estado del jugador
    int playerX = -1;
    int playerY = -1;
    String playerRole = "";
    int[] resource1 = null; // posicion recurso 1 (solo defensores la ven)
    int[] resource2 = null; // posicion recurso 2

    // Componentes de la ventana
    JPanel mapPanel;
    JTextArea logArea;
    JLabel labelStatus;
    JLabel labelPos;
    JLabel labelRole;
    JButton btnScan;
    JButton btnAttack;
    JButton btnDefend;
    JButton btnStatus;
    JButton btnQuit;

    public static void main(String[] args) {
        // Verificar que se pasen host y puerto
        if (args.length < 2) {
            JOptionPane.showMessageDialog(null,
                "Uso: java Client <host> <puerto>\nEjemplo: java Client game.servidor.com 8080",
                "Argumentos requeridos", JOptionPane.ERROR_MESSAGE);
            return;
        }

        String host = args[0];
        int puerto = Integer.parseInt(args[1]);

        // Pedir nombre y sala al usuario
        String nombre = JOptionPane.showInputDialog("Ingresa tu nombre de jugador:");
        if (nombre == null || nombre.trim().isEmpty()) return;

        String salaStr = JOptionPane.showInputDialog("ID de sala (0 para unirse/crear automáticamente):", "0");
        if (salaStr == null) return;
        int salaId;
        try {
            salaId = Integer.parseInt(salaStr.trim());
        } catch (NumberFormatException ex) {
            salaId = 0;
        }

        // Crear y mostrar la ventana
        Client cliente = new Client();
        cliente.setVisible(true);
        cliente.conectar(host, puerto, nombre, salaId);
    }

    public Client() {
        setTitle("GCP Client - Juego Multijugador");
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        construirVentana();
        pack();
        setLocationRelativeTo(null);
    }

    // Construye todos los componentes de la ventana
    void construirVentana() {
        setLayout(new BorderLayout(5, 5));

        // Panel del mapa (centro)
        mapPanel = new JPanel() {
            @Override
            protected void paintComponent(Graphics g) {
                super.paintComponent(g);
                dibujarMapa(g);
            }
        };
        mapPanel.setPreferredSize(new Dimension(GRID_SIZE * CELL_SIZE, GRID_SIZE * CELL_SIZE));
        mapPanel.setBackground(Color.BLACK);
        add(mapPanel, BorderLayout.CENTER);

        // Panel derecho: controles y log
        JPanel panelDerecho = new JPanel(new BorderLayout(5, 5));
        panelDerecho.setPreferredSize(new Dimension(220, 0));
        panelDerecho.setBorder(BorderFactory.createEmptyBorder(5, 5, 5, 5));

        panelDerecho.add(construirPanelControles(), BorderLayout.NORTH);
        panelDerecho.add(construirPanelLog(), BorderLayout.CENTER);
        add(panelDerecho, BorderLayout.EAST);

        // Barra de estado abajo
        JPanel barraEstado = new JPanel(new FlowLayout(FlowLayout.LEFT));
        labelStatus = new JLabel("Desconectado");
        labelPos    = new JLabel("Pos: -");
        labelRole   = new JLabel("Rol: -");
        barraEstado.add(labelStatus);
        barraEstado.add(new JSeparator(JSeparator.VERTICAL));
        barraEstado.add(labelPos);
        barraEstado.add(new JSeparator(JSeparator.VERTICAL));
        barraEstado.add(labelRole);
        add(barraEstado, BorderLayout.SOUTH);

        // Teclas de movimiento con el teclado
        mapPanel.setFocusable(true);
        mapPanel.addKeyListener(new KeyAdapter() {
            @Override
            public void keyPressed(KeyEvent e) {
                manejarTecla(e);
            }
        });
    }

    JPanel construirPanelControles() {
        JPanel panel = new JPanel(new GridLayout(0, 1, 4, 4));
        panel.setBorder(BorderFactory.createTitledBorder("Controles"));

        // Botones de movimiento en forma de cruceta
        JPanel movimiento = new JPanel(new GridLayout(2, 3, 2, 2));
        movimiento.add(new JLabel("")); // espacio esquina
        JButton btnArriba  = new JButton("^");
        JButton btnAbajo   = new JButton("v");
        JButton btnIzq     = new JButton("<");
        JButton btnDer     = new JButton(">");
        movimiento.add(btnArriba);
        movimiento.add(new JLabel(""));
        movimiento.add(btnIzq);
        movimiento.add(btnAbajo);
        movimiento.add(btnDer);
        panel.add(movimiento);

        btnArriba.addActionListener(e -> enviarMovimiento("UP"));
        btnAbajo.addActionListener(e  -> enviarMovimiento("DOWN"));
        btnIzq.addActionListener(e    -> enviarMovimiento("LEFT"));
        btnDer.addActionListener(e    -> enviarMovimiento("RIGHT"));

        // Botones de accion del protocolo GCP
        btnScan   = new JButton("SCAN");
        btnAttack = new JButton("ATTACK");
        btnDefend = new JButton("DEFEND");
        btnStatus = new JButton("STATUS");
        btnQuit   = new JButton("QUIT");

        btnScan.addActionListener(e   -> enviarComando("SCAN"));
        btnAttack.addActionListener(e -> pedirYEnviarRecurso("ATTACK"));
        btnDefend.addActionListener(e -> pedirYEnviarRecurso("DEFEND"));
        btnStatus.addActionListener(e -> enviarComando("STATUS"));
        btnQuit.addActionListener(e   -> enviarComando("QUIT"));

        panel.add(btnScan);
        panel.add(btnAttack);
        panel.add(btnDefend);
        panel.add(btnStatus);
        panel.add(btnQuit);

        // Deshabilitar hasta que se conecte
        habilitarBotones(false);

        return panel;
    }

    JPanel construirPanelLog() {
        JPanel panel = new JPanel(new BorderLayout());
        panel.setBorder(BorderFactory.createTitledBorder("Log de mensajes GCP"));

        logArea = new JTextArea();
        logArea.setEditable(false);
        logArea.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 11));
        logArea.setBackground(new Color(20, 20, 20));
        logArea.setForeground(new Color(180, 255, 180));
        logArea.setLineWrap(true);

        JScrollPane scroll = new JScrollPane(logArea);
        panel.add(scroll, BorderLayout.CENTER);
        return panel;
    }

    // Dibuja el mapa: cuadricula, jugador y recursos criticos
    void dibujarMapa(Graphics g) {
        int ancho = GRID_SIZE * CELL_SIZE;
        int alto  = GRID_SIZE * CELL_SIZE;

        // Fondo oscuro
        g.setColor(new Color(15, 15, 20));
        g.fillRect(0, 0, ancho, alto);

        // Cuadricula
        g.setColor(new Color(40, 40, 55));
        for (int i = 0; i <= GRID_SIZE; i++) {
            g.drawLine(i * CELL_SIZE, 0, i * CELL_SIZE, alto);
            g.drawLine(0, i * CELL_SIZE, ancho, i * CELL_SIZE);
        }

        // Dibujar recursos criticos en amarillo (solo si los conocemos)
        if (resource1 != null) {
            g.setColor(new Color(255, 200, 0));
            int rx = resource1[0] * CELL_SIZE + 4;
            int ry = resource1[1] * CELL_SIZE + 4;
            g.fillRect(rx, ry, CELL_SIZE - 8, CELL_SIZE - 8);
            g.setColor(Color.BLACK);
            g.drawString("S", rx + 5, ry + 14);
        }
        if (resource2 != null) {
            g.setColor(new Color(255, 200, 0));
            int rx = resource2[0] * CELL_SIZE + 4;
            int ry = resource2[1] * CELL_SIZE + 4;
            g.fillRect(rx, ry, CELL_SIZE - 8, CELL_SIZE - 8);
            g.setColor(Color.BLACK);
            g.drawString("S", rx + 5, ry + 14);
        }

        // Dibujar al jugador: rojo si es atacante, azul si es defensor
        if (playerX >= 0 && playerY >= 0) {
            if (playerRole.equals("ATTACKER")) {
                g.setColor(new Color(220, 60, 60));
            } else {
                g.setColor(new Color(60, 130, 220));
            }
            int px = playerX * CELL_SIZE + 3;
            int py = playerY * CELL_SIZE + 3;
            g.fillOval(px, py, CELL_SIZE - 6, CELL_SIZE - 6);

            // Letra del rol
            g.setColor(Color.WHITE);
            String inicial = playerRole.isEmpty() ? "?" : playerRole.substring(0, 1);
            g.drawString(inicial, px + 7, py + 15);
        }
    }

    // Conecta al servidor resolviendo el hostname (sin IPs hardcodeadas)
    void conectar(String host, int puerto, String nombre, int salaId) {
        agregarLog("Resolviendo hostname: " + host);

        // Resolucion DNS - si falla, se avisa pero el programa no se cierra
        InetAddress direccion;
        try {
            direccion = InetAddress.getByName(host);
            agregarLog("Host resuelto: " + direccion.getHostAddress());
        } catch (UnknownHostException e) {
            agregarLog("ERROR: No se pudo resolver '" + host + "'");
            JOptionPane.showMessageDialog(this,
                "No se pudo resolver el hostname: " + host,
                "Error DNS", JOptionPane.ERROR_MESSAGE);
            return; // el programa sigue corriendo, no se cae
        }

        // La conexion se hace en un hilo separado para no bloquear la GUI
        final InetAddress dirFinal = direccion;
        Thread hiloConexion = new Thread(() -> {
            try {
                socket = new Socket(dirFinal, puerto);
                input  = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                output = new PrintWriter(socket.getOutputStream(), true);

                // Actualizar la interfaz desde el hilo de swing
                SwingUtilities.invokeLater(() -> {
                    labelStatus.setText("Conectado");
                    labelStatus.setForeground(new Color(0, 180, 0));
                    agregarLog("Conexion exitosa a " + host + ":" + puerto);
                    habilitarBotones(true);
                });

                // Enviar JOIN con sala_id y nombre (el servidor asigna el rol)
                String msgJoin = "JOIN " + salaId + " " + nombre;
                output.println(msgJoin);
                SwingUtilities.invokeLater(() -> agregarLog("-> " + msgJoin));

                // Quedarse escuchando respuestas del servidor
                escucharServidor();

            } catch (IOException e) {
                SwingUtilities.invokeLater(() ->
                    agregarLog("ERROR al conectar: " + e.getMessage())
                );
            }
        });
        hiloConexion.setDaemon(true);
        hiloConexion.start();
    }

    // Escucha mensajes del servidor en un bucle
    void escucharServidor() {
        try {
            String linea;
            while ((linea = input.readLine()) != null) {
                final String mensaje = linea;
                SwingUtilities.invokeLater(() -> procesarRespuesta(mensaje));
            }
        } catch (IOException e) {
            SwingUtilities.invokeLater(() -> {
                agregarLog("Conexion cerrada: " + e.getMessage());
                labelStatus.setText("Desconectado");
                labelStatus.setForeground(Color.RED);
                habilitarBotones(false);
            });
        }
    }

    // Interpreta cada mensaje que llega del servidor
    void procesarRespuesta(String mensaje) {
        agregarLog("<- " + mensaje);

        // Respuesta al JOIN: servidor confirma rol y posicion inicial
        // Ejemplo: "200 OK JOINED ROOM:1 ROLE:ATTACKER POS:0,0"
        // Defensor: "200 OK JOINED ROOM:1 ROLE:DEFENDER POS:3,12 RESOURCES:5,7;14,2"
        if (mensaje.startsWith("200") && mensaje.contains("JOINED")) {
            if (mensaje.contains("ROLE:ATTACKER")) {
                playerRole = "ATTACKER";
            } else {
                playerRole = "DEFENDER";
            }

            int[] pos = extraerPos(mensaje);
            playerX = pos[0];
            playerY = pos[1];

            // Si somos defensor, el servidor nos manda donde estan los recursos
            if (mensaje.contains("RESOURCES:")) {
                parsearRecursos(mensaje);
            }

            labelRole.setText("Rol: " + playerRole);
            labelPos.setText("Pos: (" + playerX + "," + playerY + ")");
            mapPanel.repaint();
            return;
        }

        // Respuesta al MOVE: servidor confirma la nueva posicion
        // Ejemplo: "200 OK MOVE_ACCEPTED POS:6,8"
        if (mensaje.startsWith("200") && mensaje.contains("MOVE_ACCEPTED")) {
            int[] pos = extraerPos(mensaje);
            playerX = pos[0];
            playerY = pos[1];
            labelPos.setText("Pos: (" + playerX + "," + playerY + ")");
            mapPanel.repaint();
            return;
        }

        // SCAN encontro un recurso cercano
        if (mensaje.startsWith("201 FOUND")) {
            JOptionPane.showMessageDialog(this,
                "Recurso critico detectado!\n" + mensaje,
                "SCAN", JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        // Notificacion del servidor: ataque, defensa o fin de juego
        if (mensaje.startsWith("NOTIFY")) {
            if (mensaje.contains("ATTACK_STARTED")) {
                JOptionPane.showMessageDialog(this,
                    "Ataque en curso!\n" + mensaje,
                    "ALERTA", JOptionPane.WARNING_MESSAGE);
            } else if (mensaje.contains("GAME_OVER")) {
                JOptionPane.showMessageDialog(this,
                    "Partida terminada\n" + mensaje,
                    "FIN", JOptionPane.INFORMATION_MESSAGE);
            }
        }
    }

    // Extrae un numero entero de un mensaje dado una clave
    // Ejemplo: extraerEntero("200 OK X:14 Y:3", "X:") devuelve 14
    int extraerEntero(String mensaje, String clave) {
        int posicion = mensaje.indexOf(clave);
        if (posicion < 0) return -1;

        int inicio = posicion + clave.length();
        int fin = inicio;
        while (fin < mensaje.length() && Character.isDigit(mensaje.charAt(fin))) {
            fin++;
        }

        try {
            return Integer.parseInt(mensaje.substring(inicio, fin));
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    // Parsea las posiciones de los recursos del mensaje JOIN del defensor
    // Formato: RESOURCES:x1,y1;x2,y2
    void parsearRecursos(String mensaje) {
        int inicio = mensaje.indexOf("RESOURCES:") + 10;
        String resto = mensaje.substring(inicio);

        // Quitar lo que venga despues (si hay mas campos)
        if (resto.contains(" ")) {
            resto = resto.substring(0, resto.indexOf(" "));
        }

        String[] recursos = resto.split(";");

        if (recursos.length >= 1) {
            String[] xy = recursos[0].split(",");
            if (xy.length == 2) {
                int x = Integer.parseInt(xy[0].trim());
                int y = Integer.parseInt(xy[1].trim());
                resource1 = new int[]{x, y};
            }
        }
        if (recursos.length >= 2) {
            String[] xy = recursos[1].split(",");
            if (xy.length == 2) {
                int x = Integer.parseInt(xy[0].trim());
                int y = Integer.parseInt(xy[1].trim());
                resource2 = new int[]{x, y};
            }
        }
    }

    void enviarMovimiento(String direccion) {
        if (output == null) return;
        String cmd = "MOVE " + direccion;
        output.println(cmd);
        agregarLog("-> " + cmd);
    }

    void enviarComando(String cmd) {
        if (output == null) return;
        output.println(cmd);
        agregarLog("-> " + cmd);
    }

    void habilitarBotones(boolean habilitar) {
        btnScan.setEnabled(habilitar);
        btnAttack.setEnabled(habilitar);
        btnDefend.setEnabled(habilitar);
        btnStatus.setEnabled(habilitar);
        btnQuit.setEnabled(habilitar);
    }

    void manejarTecla(KeyEvent e) {
        switch (e.getKeyCode()) {
            case KeyEvent.VK_UP:    enviarMovimiento("UP");    break;
            case KeyEvent.VK_DOWN:  enviarMovimiento("DOWN");  break;
            case KeyEvent.VK_LEFT:  enviarMovimiento("LEFT");  break;
            case KeyEvent.VK_RIGHT: enviarMovimiento("RIGHT"); break;
        }
    }

    // Extrae las coordenadas del campo POS:x,y de un mensaje
    // Ejemplo: extraerPos("200 OK MOVE_ACCEPTED POS:6,8") devuelve [6, 8]
    int[] extraerPos(String mensaje) {
        int idx = mensaje.indexOf("POS:");
        if (idx < 0) return new int[]{-1, -1};
        String resto = mensaje.substring(idx + 4);
        // Cortar en espacio o fin de linea
        int fin = resto.indexOf(' ');
        if (fin >= 0) resto = resto.substring(0, fin);
        resto = resto.trim();
        String[] partes = resto.split(",");
        try {
            int x = Integer.parseInt(partes[0]);
            int y = Integer.parseInt(partes[1]);
            return new int[]{x, y};
        } catch (Exception e) {
            return new int[]{-1, -1};
        }
    }

    // Pide el ID del recurso al usuario y envía ATTACK o DEFEND con ese ID
    void pedirYEnviarRecurso(String accion) {
        String idStr = JOptionPane.showInputDialog(this, "ID del recurso:", accion, JOptionPane.QUESTION_MESSAGE);
        if (idStr == null || idStr.trim().isEmpty()) return;
        try {
            int id = Integer.parseInt(idStr.trim());
            enviarComando(accion + " " + id);
        } catch (NumberFormatException ex) {
            JOptionPane.showMessageDialog(this, "ID inválido.", "Error", JOptionPane.ERROR_MESSAGE);
        }
    }

    void agregarLog(String texto) {
        logArea.append(texto + "\n");
        logArea.setCaretPosition(logArea.getDocument().getLength());
    }
}