#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    char buffer[BUFFER] = {0};
    int addrlen = sizeof(address);

    // 1. Crear el socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Error creando socket");
        exit(1);
    }

    // 2. Configurar la direccion y puerto
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // Acepta conexiones de cualquier IP
    address.sin_port = htons(PORT);

    // 3. Bind: asociar el socket al puerto
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Error en bind");
        exit(1);
    }

    // 4. Listen: ponerse a escuchar (max 3 en cola)
    if (listen(server_fd, 3) < 0) {
        perror("Error en listen");
        exit(1);
    }

    printf("Servidor escuchando en el puerto %d...\n", PORT);

    // 5. Accept: aceptar la conexion de un cliente
    client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if (client_fd < 0) {
        perror("Error en accept");
        exit(1);
    }

    printf("Cliente conectado!\n");

    // 6. Recibir mensaje del cliente
    read(client_fd, buffer, BUFFER);
    printf("Mensaje recibido: %s\n", buffer);

    // 7. Responder al cliente
    char* respuesta = "200 OK HOLA DESDE EL SERVIDOR\n";
    send(client_fd, respuesta, strlen(respuesta), 0);
    printf("Respuesta enviada.\n");

    // 8. Cerrar conexiones
    close(client_fd);
    close(server_fd);

    return 0;
}
