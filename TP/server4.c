#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
// ### [AJOUT] Bibliothèque pour l'horloge monotone (clock_gettime)
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9004
// ### [MODIF] Taille de bloc de 64 Kio (65 536 octets) comme demandé
#define CHUNK_SIZE (64 * 1024)

// ### [AJOUT] Récupère le temps précis en secondes avec une horloge monotone
static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void) {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) { perror("socket"); return EXIT_FAILURE; }

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);

    if (bind(sfd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("bind"); close(sfd); return EXIT_FAILURE;
    }
    if (listen(sfd, 5) == -1) {
        perror("listen"); close(sfd); return EXIT_FAILURE;
    }

    printf("[Serveur Débit] En attente sur 127.0.0.1:%d...\n", PORT);
    int cfd = accept(sfd, NULL, NULL);
    if (cfd == -1) { perror("accept"); close(sfd); return EXIT_FAILURE; }

    // ### [AJOUT] Allocation dynamique d'un buffer de 64 Kio et compteurs
    char *buf = malloc(CHUNK_SIZE);
    if (!buf) { perror("malloc"); close(cfd); close(sfd); return EXIT_FAILURE; }

    size_t total_bytes = 0;
    double start = 0.0;

    // ### [MODIF] Réception en continu jusqu'à ce que le client ferme son canal d'écriture (recv == 0)
    while (1) {
        ssize_t n = recv(cfd, buf, CHUNK_SIZE, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { perror("recv"); break; }
        if (n == 0) break; // Fin d'émission signalée par shutdown(SHUT_WR) côté client

        // Démarre le chrono dès l'arrivée du tout premier paquet de données
        if (total_bytes == 0) start = get_time_sec();
        total_bytes += (size_t)n;
    }
    double end = get_time_sec();

    // ### [AJOUT] Calcul de la durée et du débit en Mbit/s (octets * 8 / 1 000 000)
    double duration = end - start;
    double mbps = (duration > 0.0) ? ((double)total_bytes * 8.0) / (duration * 1e6) : 0.0;

    char report[256];
    snprintf(report, sizeof(report),
             "Bilan serveur : %zu octets reçus en %.4f s -> %.2f Mbit/s\n",
             total_bytes, duration, mbps);

    // Envoie le bilan textuel au client avant de fermer la connexion
    send(cfd, report, strlen(report), 0);
    printf("%s", report);

    free(buf);
    close(cfd);
    close(sfd);
    return EXIT_SUCCESS;
}