#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9002
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

// ### [AJOUT] Lit octet par octet jusqu'au '\n' (protocole de délimitation)
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

int main(void) {
    int sfd, cfd;
    struct sockaddr_in serv, cli;
    socklen_t clilen = sizeof(cli);

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) { perror("socket"); return EXIT_FAILURE; }

    // ### [AJOUT] Option pour relancer le serveur immédiatement
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    // ### [MODIF] Écoute uniquement sur 127.0.0.1
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);
    serv.sin_port = htons(PORT);

    if (bind(sfd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("bind"); close(sfd); return EXIT_FAILURE;
    }
    if (listen(sfd, 5) == -1) {
        perror("listen"); close(sfd); return EXIT_FAILURE;
    }
    printf("[Serveur] En attente sur 127.0.0.1:%d...\n", PORT);

    cfd = accept(sfd, (struct sockaddr *)&cli, &clilen);
    if (cfd == -1) { perror("accept"); close(sfd); return EXIT_FAILURE; }

    char buf[BUF_SIZE];
    // ### [AJOUT] Buffer dédié pour le préfixe "Serveur : "
    char reply[BUF_SIZE + 32];

    for (;;) {
        // ### [MODIF] Réception délimitée par '\n'
        ssize_t n = recv_line(cfd, buf, sizeof(buf));
        if (n <= 0) break;

        // ### [AJOUT] Commande /quit
        if (strcmp(buf, "/quit\n") == 0 || strcmp(buf, "/quit\r\n") == 0) {
            printf("[Serveur] Commande /quit reçue.\n");
            break;
        }

        // ### [MODIF] Réponse formatée
        snprintf(reply, sizeof(reply), "Serveur : %s", buf);
        if (send_all(cfd, reply, strlen(reply)) == -1) {
            perror("send"); break;
        }
    }

    close(cfd);
    close(sfd);
    return EXIT_SUCCESS;
}