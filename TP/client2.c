#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

// ### [AJOUT] Lecture d'une réponse délimitée par '\n'
static ssize_t recv_line(int fd, char *buf, size_t max_len) {
    size_t pos = 0;
    while (pos < max_len - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return n;
        buf[pos++] = c;
        if (c == '\n') break;
    }
    buf[pos] = '\0';
    return (ssize_t)pos;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage : %s <IP_serveur> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

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

    char buf[BUF_SIZE];
    // ### [AJOUT] Boucle infinie pour la conversation persistante
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;

        // ### [MODIF] Conservation du '\n' comme délimiteur
        if (send_all(sfd, buf, strlen(buf)) == -1) {
            perror("send"); break;
        }

        // ### [AJOUT] Arrêt immédiat sur /quit
        if (strcmp(buf, "/quit\n") == 0) break;

        // ### [MODIF] Réception ligne par ligne
        ssize_t n = recv_line(sfd, buf, sizeof(buf));
        if (n <= 0) {
            printf("Connexion fermée par le serveur.\n");
            break;
        }
        printf("%s", buf);
    }

    close(sfd);
    return EXIT_SUCCESS;
}