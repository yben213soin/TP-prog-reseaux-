#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
// ### [AJOUT] Bibliothèque pour l'horloge monotone
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CHUNK_SIZE (64 * 1024) // 64 Kio

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
    // ### [MODIF] Prend 3 arguments : IP, port et volume en Mio (ex: 32)
    if (argc != 4) {
        fprintf(stderr, "Usage : %s <IP> <port> <taille_en_Mio>\n", argv[0]);
        return EXIT_FAILURE;
    }

    long mib = strtol(argv[3], NULL, 10);
    if (mib <= 0) {
        fprintf(stderr, "Taille invalide : %s\n", argv[3]);
        return EXIT_FAILURE;
    }
    size_t total_to_send = (size_t)mib * 1024 * 1024;

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons((uint16_t)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &serv.sin_addr) != 1) {
        fprintf(stderr, "IP invalide\n"); close(sfd); return EXIT_FAILURE;
    }

    if (connect(sfd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("connect"); close(sfd); return EXIT_FAILURE;
    }

    // ### [MODIF] Création d'un bloc de 64 Kio rempli de 'A' au lieu de lire au clavier
    char *buf = malloc(CHUNK_SIZE);
    if (!buf) { perror("malloc"); close(sfd); return EXIT_FAILURE; }
    memset(buf, 'A', CHUNK_SIZE);

    size_t sent_total = 0;
    double start = get_time_sec();

    // ### [AJOUT] Boucle d'envoi par blocs de 64 Kio jusqu'au volume demandé
    while (sent_total < total_to_send) {
        size_t remaining = total_to_send - sent_total;
        size_t to_send = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;

        ssize_t n = send(sfd, buf, to_send, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { perror("send"); break; }
        sent_total += (size_t)n;
    }

    // ### [AJOUT] Ferme uniquement l'envoi (envoie FIN) mais garde la socket ouverte en lecture
    shutdown(sfd, SHUT_WR);
    double end = get_time_sec();

    double duration = end - start;
    double mbps = (duration > 0.0) ? ((double)sent_total * 8.0) / (duration * 1e6) : 0.0;
    printf("Bilan client  : %zu octets envoyés en %.4f s -> %.2f Mbit/s\n",
           sent_total, duration, mbps);

    // ### [AJOUT] Attente et affichage du bilan calculé par le serveur
    char report[256];
    ssize_t r = recv(sfd, report, sizeof(report) - 1, 0);
    if (r > 0) {
        report[r] = '\0';
        printf("%s", report);
    }

    free(buf);
    close(sfd);
    return EXIT_SUCCESS;
}