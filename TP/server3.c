#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
// ### [AJOUT] Bibliothèques pour les threads POSIX et les signaux
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9003
#define BUF_SIZE 1024

// ### [AJOUT] Compteur global des clients actifs et mutex pour éviter les conflits
static int active_clients = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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

// ### [AJOUT] Fonction exécutée en parallèle par chaque thread (un thread = un client)
static void *handle_client(void *arg) {
    int cfd = *(int *)arg;
    free(arg); // Libère la mémoire réservée dans main()

    // ### [AJOUT] Détache le thread pour que Linux libère sa mémoire automatiquement à sa fin
    pthread_detach(pthread_self());

    // ### [AJOUT] Section critique : on verrouille le mutex pour modifier active_clients
    pthread_mutex_lock(&lock);
    active_clients++;
    printf("[Serveur] Client connecté (fd=%d). Clients actifs : %d\n", cfd, active_clients);
    pthread_mutex_unlock(&lock);

    char buf[BUF_SIZE];
    char reply[BUF_SIZE + 32];

    while (1) {
        ssize_t n = recv_line(cfd, buf, sizeof(buf));
        if (n <= 0) {
            printf("[Serveur] Déconnexion brutale (Ctrl+C) du client (fd=%d).\n", cfd);
            break;
        }
        if (strcmp(buf, "/quit\n") == 0 || strcmp(buf, "/quit\r\n") == 0) {
            printf("[Serveur] Commande /quit reçue (fd=%d).\n", cfd);
            break;
        }

        printf("[Serveur] Reçu de fd=%d : %s", cfd, buf);
        snprintf(reply, sizeof(reply), "Serveur : %s", buf);
        if (send_all(cfd, reply, strlen(reply)) == -1) {
            perror("send");
            break;
        }
    }

    // ### [AJOUT] Décrémentation protégée du compteur avant fermeture
    pthread_mutex_lock(&lock);
    active_clients--;
    printf("[Serveur] Client déconnecté (fd=%d). Clients actifs : %d\n", cfd, active_clients);
    pthread_mutex_unlock(&lock);

    close(cfd); // Chaque thread ferme uniquement la socket de son propre client
    return NULL;
}

int main(void) {
    // ### [AJOUT] Empêche le serveur de crasher si un client coupe brutalement pendant un send()
    signal(SIGPIPE, SIG_IGN);

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
    if (listen(sfd, 16) == -1) {
        perror("listen"); close(sfd); return EXIT_FAILURE;
    }

    printf("[Serveur Multithread] En attente sur 127.0.0.1:%d...\n", PORT);

    // ### [MODIF] Boucle infinie : dès qu'un client arrive, on lui crée un thread et on repart sur accept()
    while (1) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd == -1) { perror("accept"); continue; }

        // ### [AJOUT] Allocation dynamique du descripteur pour éviter qu'un client n'écrase celui d'un autre
        int *pfd = malloc(sizeof(int));
        if (!pfd) {
            perror("malloc");
            close(cfd);
            continue;
        }
        *pfd = cfd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, pfd) != 0) {
            perror("pthread_create");
            free(pfd);
            close(cfd);
        }
    }

    close(sfd);
    return EXIT_SUCCESS;
}