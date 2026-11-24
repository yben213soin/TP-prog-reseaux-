#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
// ### [AJOUT] Bibliothèque pour struct timeval (gestion du timeout)
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage : %s <IP> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // ### [MODIF] SOCK_DGRAM (UDP) au lieu de SOCK_STREAM (TCP)
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd == -1) { perror("socket"); return EXIT_FAILURE; }

    // ### [AJOUT] Timeout de réception de 2 secondes via SO_RCVTIMEO
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if (setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        perror("setsockopt"); close(sfd); return EXIT_FAILURE;
    }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons((uint16_t)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &serv.sin_addr) != 1) {
        fprintf(stderr, "IP invalide\n"); close(sfd); return EXIT_FAILURE;
    }

    // ### [SUPPR] Pas de connect() en UDP

    char buf[BUF_SIZE];
    printf("Message UDP à envoyer : ");
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) { close(sfd); return EXIT_FAILURE; }

    // ### [MODIF] sendto() précise l'adresse du serveur à chaque envoi
    if (sendto(sfd, buf, strlen(buf), 0,
               (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("sendto"); close(sfd); return EXIT_FAILURE;
    }

    // ### [MODIF] recvfrom() attend la réponse pendant 2 secondes maximum
    ssize_t n = recvfrom(sfd, buf, sizeof(buf) - 1, 0, NULL, NULL);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "Timeout (2s) : aucune réponse du serveur.\n");
        } else {
            perror("recvfrom");
        }
    } else {
        buf[n] = '\0';
        printf("Réponse du serveur UDP : %s", buf);
    }

    close(sfd);
    return EXIT_SUCCESS;
}
