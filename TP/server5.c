#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9005
#define BUF_SIZE 1024

int main(void) {
    // ### [MODIF] SOCK_DGRAM (UDP) à la place de SOCK_STREAM (TCP)
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd == -1) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in serv, cli;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);

    if (bind(sfd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("bind"); close(sfd); return EXIT_FAILURE;
    }

    // ### [SUPPR] Pas de listen() ni d'accept() : UDP est un protocole sans connexion
    printf("[Serveur UDP Echo] En écoute sur 127.0.0.1:%d...\n", PORT);
    char buf[BUF_SIZE];

    while (1) {
        socklen_t clilen = sizeof(cli);
        // ### [MODIF] recvfrom() lit un datagramme entier et récupère l'adresse de l'expéditeur dans 'cli'
        ssize_t n = recvfrom(sfd, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr *)&cli, &clilen);
        if (n < 0) { perror("recvfrom"); continue; }

        buf[n] = '\0';
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
        printf("[Serveur UDP] Reçu de %s:%u (%zd octets) : %s",
               ip, (unsigned)ntohs(cli.sin_port), n, buf);

        // ### [MODIF] sendto() renvoie directement le datagramme à l'adresse 'cli'
        if (sendto(sfd, buf, (size_t)n, 0,
                   (struct sockaddr *)&cli, clilen) == -1) {
            perror("sendto");
        }
    }

    close(sfd);
    return EXIT_SUCCESS;
}
