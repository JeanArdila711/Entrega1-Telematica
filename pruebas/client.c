#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER 1024

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER] = {0};

    // 1. Crear el socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Error creando socket");
        exit(1);
    }

    // 2. Configurar a donde conectarse
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);  // IP del servidor

    // 3. Connect: conectarse al servidor
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error conectando");
        exit(1);
    }

    printf("Conectado al servidor!\n");

    // 4. Enviar un mensaje
    char* mensaje = "MOVE RIGHT\n";
    send(sock_fd, mensaje, strlen(mensaje), 0);
    printf("Mensaje enviado: %s", mensaje);

    // 5. Recibir respuesta
    read(sock_fd, buffer, BUFFER);
    printf("Respuesta del servidor: %s\n", buffer);

    // 6. Cerrar
    close(sock_fd);

    return 0;
}
