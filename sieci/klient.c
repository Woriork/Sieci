#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define MAX_DATA 100000

// Funkcja obsługująca błędy
void handle_error(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Funkcja licząca ilość wystąpień bitów na poszczególnych pozycjach
void count_bits(uint32_t *data, uint32_t *bitcnt) {
    for (int i = 0; i < MAX_DATA; i++) {
        for (int j = 0; j < 16; j++) {
            bitcnt[j] += (data[i] >> j) & 1;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Użycie: %s <nazwa> <adres_serwera> <nr_portu>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *client_name = argv[1];
    char *server_ip = argv[2];
    int server_port = atoi(argv[3]);

    int command_sock, data_sock;
    struct sockaddr_in server_addr, data_addr;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    // Tworzenie gniazda poleceń
    command_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (command_sock < 0) handle_error("Błąd tworzenia gniazda");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) handle_error("Nieprawidłowy adres/Adres nieobsługiwany");

    // Połączenie z serwerem
    if (connect(command_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) handle_error("Błąd połączenia");

    // Wysłanie polecenia do serwera
    snprintf(buffer, BUFFER_SIZE, "N%s", client_name);
    if (send(command_sock, buffer, strlen(buffer), 0) < 0) handle_error("Błąd wysyłania");

    printf("Wysłano polecenie %s do serwera.\n", buffer);

    // Odbiór odpowiedzi od serwera
    bytes_read = recv(command_sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read < 0) handle_error("Błąd odbioru");
    buffer[bytes_read] = '\0';
    printf("Otrzymano: %s\n", buffer);

    if (buffer[0] == '@' && buffer[11] == 'P') {
        int data_port = atoi(&buffer[13]);
        printf("Otrzymano port danych: %d\n", data_port);

        // Tworzenie gniazda danych
        data_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (data_sock < 0) handle_error("Błąd tworzenia gniazda danych");

        data_addr.sin_family = AF_INET;
        data_addr.sin_port = htons(data_port);

        if (inet_pton(AF_INET, server_ip, &data_addr.sin_addr) <= 0) handle_error("Nieprawidłowy adres/Adres nieobsługiwany");

        // Połączenie z gniazdem danych
        if (connect(data_sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) handle_error("Błąd połączenia danych");

        uint32_t data[MAX_DATA];
        int received = 0;
        // Odbiór danych
        while (received < MAX_DATA * sizeof(uint32_t)) {
            int bytes_received = recv(data_sock, ((char *)data) + received, 1000, 0);
            if (bytes_received < 0) {
                perror("Błąd odbioru danych");
                break;
            }
            received += bytes_received;
        }

        printf("Dane odebrane.\n");

        uint32_t bitcnt[16] = {0};
        // Liczenie bitów
        count_bits(data, bitcnt);

        snprintf(buffer, BUFFER_SIZE, "R");
        for (int i = 0; i < 16; i++) {
            char tmp[16];
            snprintf(tmp, sizeof(tmp), "%u ", bitcnt[i]);
            strncat(buffer, tmp, sizeof(buffer) - strlen(buffer) - 1);
        }

        // Wysłanie wyniku do serwera
        if (send(command_sock, buffer, strlen(buffer), 0) < 0) handle_error("Błąd wysyłania wyniku");

        printf("Wysłano wynik do serwera: %s\n", buffer);

        // Odbiór odpowiedzi od serwera
        bytes_read = recv(command_sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read < 0) handle_error("Błąd odbioru");
        buffer[bytes_read] = '\0';
        printf("Otrzymano: %s\n", buffer);

        if (buffer[0] == 'D') {
            printf("Wynik zaakceptowany przez serwer.\n");
        }
    } else if (buffer[0] == '@' && buffer[11] == 'X') {
        printf("Brak nowych danych.\n");
    }

    close(command_sock);
    return 0;
}

