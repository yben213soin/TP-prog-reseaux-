#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Compile tous les exercices (2, 3, 4, 5) automatiquement au lancement */
static int compiler_exercices(void) {
    printf("=== COMPILATION AUTOMATIQUE DES EXERCICES 2, 3, 4, 5 ===\n");
    const char *cmds[] = {
        "gcc -std=c11 -Wall -Wextra server2.c -o server2",
        "gcc -std=c11 -Wall -Wextra client2.c -o client2",
        "gcc -std=c11 -Wall -Wextra -pthread server3.c -o server3",
        "gcc -std=c11 -Wall -Wextra server4.c -o server4",
        "gcc -std=c11 -Wall -Wextra client4.c -o client4",
        "gcc -std=c11 -Wall -Wextra server5.c -o server5",
        "gcc -std=c11 -Wall -Wextra client5.c -o client5"
    };
    int nb = sizeof(cmds) / sizeof(cmds[0]);
    for (int i = 0; i < nb; i++) {
        printf("[GCC] %s\n", cmds[i]);
        if (system(cmds[i]) != 0) {
            fprintf(stderr, "[Erreur] Echec de compilation pour : %s\n", cmds[i]);
            return -1;
        }
    }
    printf("=== TOUS LES EXECUTABLES SONT PRETS ===\n\n");
    return 0;
}

/* Lance un processus en arrière-plan et retourne son PID */
static pid_t lancer_bg(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    return pid;
}

/* Arrête proprement le serveur */
static void arreter_serveur(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
}

/* Arrête proprement tcpdump avec SIGINT (Ctrl+C) pour forcer l'écriture du .pcap */
static void arreter_tcpdump(pid_t shell_pid) {
    system("sudo -n pkill -INT -x tcpdump 2>/dev/null");
    if (shell_pid > 0) {
        waitpid(shell_pid, NULL, 0);
    }
}

int main(void) {
    if (compiler_exercices() != 0) {
        return EXIT_FAILURE;
    }

    /* Demande le mot de passe sudo au premier plan AVANT de lancer tcpdump en arrière-plan */
    printf("[Auth] Autorisation sudo requise pour la capture tcpdump :\n");
    if (system("sudo -v") != 0) {
        fprintf(stderr, "[Erreur] Echec de l'authentification sudo.\n");
        return EXIT_FAILURE;
    }
    printf("\n");

    while (1) {
        system("stty sane 2>/dev/null");

        printf("==================================================\n");
        printf("   MENU DE TEST ET CAPTURE WIRESHARK (TP RESEAU)  \n");
        printf("==================================================\n");
        printf("2) Exercice 2 : Chat TCP sequentiel (port 9002)\n");
        printf("3) Exercice 3 : Serveur TCP multithread - 3 clients (port 9003)\n");
        printf("4) Exercice 4 : Mesure de debit TCP (port 9004)\n");
        printf("5) Exercice 5 : Client/Serveur Echo UDP (port 9005)\n");
        printf("0) Quitter\n");
        printf("--------------------------------------------------\n");
        printf("Choisis l'exercice a lancer (2, 3, 4, 5 ou 0) : ");
        fflush(stdout);

        int choix = -1;
        if (scanf("%d", &choix) != 1) break;
        while (getchar() != '\n');

        if (choix == 0) {
            printf("Fermeture du menu.\n");
            break;
        }

        /* Prolonge le cache sudo sans bloquer le clavier */
        system("sudo -n -v 2>/dev/null");

        const char *pcap = NULL;
        pid_t srv_pid = -1;
        pid_t cap_pid = -1;

        if (choix == 2) {
            pcap = "capture_exo2.pcap";
            srv_pid = lancer_bg("./server2");
            sleep(1);

            printf("[Capture] Demarrage de tcpdump sur lo (port 9002)...\n");
            cap_pid = lancer_bg("sudo -n tcpdump -i lo -U -s 0 tcp port 9002 -w capture_exo2.pcap </dev/null >/dev/null 2>&1");
            sleep(1);

            printf("[Test Exo 2] Envoi de 3 messages puis /quit...\n");
            system("printf 'Message 1\\nMessage 2\\nMessage 3\\n/quit\\n' | ./client2 127.0.0.1 9002");

        } else if (choix == 3) {
            pcap = "capture_exo3.pcap";
            srv_pid = lancer_bg("./server3");
            sleep(1);

            printf("[Capture] Demarrage de tcpdump sur lo (port 9003)...\n");
            cap_pid = lancer_bg("sudo -n tcpdump -i lo -U -s 0 tcp port 9003 -w capture_exo3.pcap </dev/null >/dev/null 2>&1");
            sleep(1);

            printf("[Test Exo 3] Lancement de 3 clients simultanes...\n");
            system("(sleep 0.2; printf 'Client 1\\n/quit\\n') | ./client2 127.0.0.1 9003 & "
                   "(sleep 0.4; printf 'Client 2\\n/quit\\n') | ./client2 127.0.0.1 9003 & "
                   "(sleep 0.6; printf 'Client 3\\n/quit\\n') | ./client2 127.0.0.1 9003 & "
                   "wait");

        } else if (choix == 4) {
            pcap = "capture_exo4.pcap";
            int vol = 32;
            printf("Volume a transferer en Mio (ex: 1, 32 ou 128) : ");
            fflush(stdout);
            if (scanf("%d", &vol) != 1 || vol <= 0) vol = 32;
            while (getchar() != '\n');

            srv_pid = lancer_bg("./server4");
            sleep(1);

            printf("[Capture] Demarrage de tcpdump sur lo (port 9004)...\n");
            cap_pid = lancer_bg("sudo -n tcpdump -i lo -U -s 0 tcp port 9004 -w capture_exo4.pcap </dev/null >/dev/null 2>&1");
            sleep(1);

            char cmd[128];
            snprintf(cmd, sizeof(cmd), "./client4 127.0.0.1 9004 %d", vol);
            printf("[Test Exo 4] Transfert de %d Mio...\n", vol);
            system(cmd);

        } else if (choix == 5) {
            pcap = "capture_exo5.pcap";
            srv_pid = lancer_bg("./server5");
            sleep(1);

            printf("[Capture] Demarrage de tcpdump sur lo (port 9005)...\n");
            cap_pid = lancer_bg("sudo -n tcpdump -i lo -U -s 0 udp port 9005 -w capture_exo5.pcap </dev/null >/dev/null 2>&1");
            sleep(1);

            printf("[Test Exo 5] Envoi d'un datagramme UDP...\n");
            system("printf 'Test datagramme UDP\\n' | ./client5 127.0.0.1 9005");

        } else {
            printf("Choix invalide.\n\n");
            continue;
        }

        sleep(1);
        arreter_tcpdump(cap_pid);
        arreter_serveur(srv_pid);

        /* Restaure immédiatement le mode normal du terminal avant d'afficher l'aperçu */
        system("stty sane 2>/dev/null");

        printf("\n==================================================\n");
        printf("   APERCU DU CONTENU DES DONNEES CAPTUREES         \n");
        printf("==================================================\n");
        char view_cmd[128];
        snprintf(view_cmd, sizeof(view_cmd), "tcpdump -r %s -nn -A 2>/dev/null | head -n 45", pcap);
        system(view_cmd);

        printf("\n[Wireshark] Ouvrir %s dans Wireshark ? (o/n) : ", pcap);
        fflush(stdout);
        int rep = getchar();
        while (rep != '\n' && getchar() != '\n');
        if (rep == 'o' || rep == 'O') {
            char ws_cmd[128];
            snprintf(ws_cmd, sizeof(ws_cmd), "wireshark %s </dev/null >/dev/null 2>&1 &", pcap);
            system(ws_cmd);
        }
        printf("\n");
    }

    return EXIT_SUCCESS;
}