//chmod +x start_clients.sh
//./start_clients.sh
//./client pedro 127.0.0.1 5050
//./server 5050

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define MAX_CONNECTION 10
#define MAX_DATA 100000
#define BUFFER_SIZE 1024

struct CALCDATA {
    uint32_t data[MAX_DATA];
};

void create_data(struct CALCDATA *cdata) {
    for (uint32_t i = 0; i < MAX_DATA; i++) {
        cdata->data[i] = rand() & 0x0000FFFF;
    }
}

void handle_error(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Użycie: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_socket;
    int client_sockets[MAX_CONNECTION] = {0};
    int data_sockets[MAX_CONNECTION] = {0};
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    fd_set readfds;

    struct CALCDATA cdata[MAX_CONNECTION];
    for (int i = 0; i < MAX_CONNECTION; i++) {
        create_data(&cdata[i]);
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) handle_error("Tworzenie gniazda nie powiodło się");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        handle_error("Bindowanie nie powiodło się");
    }

    if (listen(server_socket, MAX_CONNECTION) < 0) {
        handle_error("Nasłuchiwanie nie powiodło się");
    }

    printf("Serwer nasłuchuje na porcie %d...\n", port);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        int max_sd = server_socket;

        for (int i = 0; i < MAX_CONNECTION; i++) {
            if (client_sockets[i] > 0) {
                FD_SET(client_sockets[i], &readfds);
            }
            if (data_sockets[i] > 0) {
                FD_SET(data_sockets[i], &readfds);
            }
            if (client_sockets[i] > max_sd) {
                max_sd = client_sockets[i];
            }
            if (data_sockets[i] > max_sd) {
                max_sd = data_sockets[i];
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            handle_error("Błąd funkcji select");
        }

        if (FD_ISSET(server_socket, &readfds)) {
            int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
            if (new_socket < 0) {
                handle_error("Akceptowanie połączenia nie powiodło się");
            }

            printf("Nowe połączenie z %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            for (int i = 0; i < MAX_CONNECTION; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CONNECTION; i++) {
            if (FD_ISSET(client_sockets[i], &readfds)) {
                char buffer[BUFFER_SIZE];
                int valread = recv(client_sockets[i], buffer, BUFFER_SIZE - 1, 0);
                if (valread <= 0) {
                    close(client_sockets[i]);
                    client_sockets[i] = 0;
                } else {
                    buffer[valread] = '\0';
                    printf("Odebrano polecenie: %s\n", buffer);

                    if (buffer[0] == 'N') {
                        struct sockaddr_in data_addr;
                        memset(&data_addr, 0, sizeof(data_addr));
                        data_addr.sin_family = AF_INET;
                        data_addr.sin_addr.s_addr = INADDR_ANY;
                        data_addr.sin_port = htons(0); // Dynamicznie alokowany port

                        data_sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
                        if (data_sockets[i] < 0) {
                            handle_error("Tworzenie gniazda danych nie powiodło się");
                        }

                        if (bind(data_sockets[i], (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
                            handle_error("Bindowanie gniazda danych nie powiodło się");
                        }

                        socklen_t len = sizeof(data_addr);
                        if (getsockname(data_sockets[i], (struct sockaddr *)&data_addr, &len) == -1) {
                            handle_error("Błąd getsockname");
                        }

                        if (listen(data_sockets[i], 1) < 0) {
                            handle_error("Nasłuchiwanie na gnieździe danych nie powiodło się");
                        }

                        char response[BUFFER_SIZE];
                        snprintf(response, sizeof(response), "@000000000!P:%d", ntohs(data_addr.sin_port));
                        send(client_sockets[i], response, strlen(response), 0);
                        printf("Wysłano port danych %d do klienta\n", ntohs(data_addr.sin_port));
                    }
                }
            }

            if (FD_ISSET(data_sockets[i], &readfds)) {
                int data_client_socket = accept(data_sockets[i], (struct sockaddr *)&client_addr, &addr_len);
                if (data_client_socket < 0) {
                    handle_error("Akceptowanie połączenia danych nie powiodło się");
                }

                printf("Połączenie danych ustanowione na porcie %d\n", ntohs(client_addr.sin_port));

                int sent = 0;
                while (sent < MAX_DATA * sizeof(uint32_t)) {
                    int bytes_sent = send(data_client_socket, ((char *)cdata[i].data) + sent, 1000, 0);
                    if (bytes_sent < 0) {
                        perror("Błąd wysyłania danych");
                        break;
                    }
                    sent += bytes_sent;
                }

                close(data_client_socket);
                close(data_sockets[i]);
                data_sockets[i] = 0;

                printf("Dane wysłane do klienta\n");

                char buffer[BUFFER_SIZE];
                int valread = recv(client_sockets[i], buffer, BUFFER_SIZE - 1, 0);
                if (valread <= 0) {
                    close(client_sockets[i]);
                    client_sockets[i] = 0;
                } else {
                    buffer[valread] = '\0';
                    printf("Odebrano wynik: %s\n", buffer);

                    if (buffer[0] == 'R') {
                        printf("Wyniki odebrane od klienta: %s\n", buffer + 1);
                        send(client_sockets[i], "D", 1, 0);
                        close(client_sockets[i]);
                        client_sockets[i] = 0;
                    } else if (buffer[0] == 'E') {
                        printf("Błąd zgłoszony przez klienta\n");
                        close(client_sockets[i]);
                        client_sockets[i] = 0;
                    }
                }
            }
        }
    }

    close(server_socket);
    return 0;
}

