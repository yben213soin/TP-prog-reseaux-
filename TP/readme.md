# Documentation Technique Complète — TP Programmation Réseaux (Sockets C / POSIX)

Ce document détaille l'architecture interne, la gestion de la mémoire (buffers), les bibliothèques et chaque appel système utilisé dans chacun des fichiers du projet (`server2.c`, `client2.c`, `server3.c`, `server4.c`, `client4.c`, `server5.c`, `client5.c`, `test_tp.sh` et `menu.c`).

---

## 1. Vue d'ensemble des bibliothèques C et de leur rôle exact

Avant d'analyser chaque fichier, voici le rôle précis des en-têtes système utilisés dans le projet :

* **`#define _POSIX_C_SOURCE 200809L`** : Active les fonctionnalités POSIX modernes (comme `clock_gettime`, `kill`, `sigaction`) lorsque le compilateur est bridé en mode strict `-std=c11`.
* **`<sys/socket.h>`** : Cœur de l'API Sockets BSD/POSIX. Définit les fonctions `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`, `sendto()`, `recvfrom()`, `shutdown()`, `setsockopt()`, ainsi que les types `socklen_t` et les constantes `AF_INET` (IPv4), `SOCK_STREAM` (TCP), `SOCK_DGRAM` (UDP).
* **`<netinet/in.h>`** : Définit la structure d'adresse IPv4 `struct sockaddr_in` (`sin_family`, `sin_port`, `sin_addr`) et les fonctions de conversion d'endianness (ordre des octets en mémoire) :
  * `htons()` (*Host To Network Short*) : Convertit un entier 16 bits (port) de l'ordre processeur (Little-Endian sur x86) vers l'ordre réseau (Big-Endian).
  * `ntohs()` (*Network To Host Short*) : Conversion inverse pour afficher le port d'un client.
* **`<arpa/inet.h>`** : Convertit les adresses IP entre le format texte lisible par l'humain et le format binaire 32 bits :
  * `inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr)` (*Presentation to Network*).
  * `inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip))` (*Network to Presentation*).
* **`<unistd.h>`** : Appels système UNIX standard : `close()` (fermeture d'un descripteur de socket), `fork()`, `execl()`, `sleep()`.
* **`<errno.h>`** : Expose la variable globale `errno`, mise à jour par le noyau Linux dès qu'un appel système retourne `-1`. Permet de distinguer une vraie erreur fatale d'une interruption temporaire (`EINTR`) ou d'un timeout (`EAGAIN` / `EWOULDBLOCK`).
* **`<pthread.h>`** *(Exercice 3)* : Gestion des threads légers POSIX (`pthread_create`, `pthread_detach`, `pthread_mutex_lock`, `pthread_mutex_unlock`).
* **`<signal.h>`** *(Exercice 3 & `menu.c`)* : Permet d'ignorer `SIGPIPE` (`signal(SIGPIPE, SIG_IGN)`) ou d'envoyer `SIGTERM` à un processus fils.
* **`<time.h>`** *(Exercice 4)* : Fournit `struct timespec` et `clock_gettime(CLOCK_MONOTONIC, &ts)` pour chronométrer les transferts à la nanoseconde près.
* **`<sys/time.h>`** *(Exercice 5)* : Définit `struct timeval` (`tv_sec`, `tv_usec`) passé à `setsockopt(..., SO_RCVTIMEO, ...)` pour imposer un timeout sur les sockets UDP.
* **`<sys/wait.h>`** *(`menu.c`)* : Fournit `waitpid()` pour attendre la fin propre des processus fils (`serverX` et `tcpdump`) et éviter les processus zombies.

---

## 2. Analyse détaillée fichier par fichier

---

### A. Exercice 2 — Chat TCP séquentiel (`server2.c` et `client2.c`)

#### 1. `server2.c` (Serveur TCP monoclients sur `127.0.0.1:9002`)
* **Fonctions utilitaires internes :**
  * `send_all(int fd, const char *buf, size_t len)` : Boucle `while (sent < len)` autour de `send()`. Comme TCP est un flux continu d'octets, le noyau peut n'accepter qu'une partie du buffer si sa mémoire tampon est saturée. `send_all()` décale le pointeur (`buf + sent`) jusqu'à ce que la totalité des `len` octets soit transmise.
  * `recv_line(int fd, char *buf, size_t max_len)` : Lit la socket **1 octet à la fois** (`recv(fd, &c, 1, 0)`) jusqu'à rencontrer le délimiteur `'\n'` ou atteindre `max_len - 1`. Elle place ensuite `'\0'` à la fin pour former une chaîne C valide. Cela garantit qu'un message applicatif n'est jamais coupé ni collé au suivant.
* **Variables et Buffers :**
  * `int sfd` : Descripteur de la **socket d'écoute** (passive).
  * `int cfd` : Descripteur de la **socket connectée** (active, créée par `accept()`).
  * `char buf[1024]` : Tampon de réception stockant la ligne envoyée par le client.
  * `char reply[1056]` (`BUF_SIZE + 32`) : Tampon d'émission agrandi de 32 octets pour accueillir le préfixe `"Serveur : "` devant `buf` sans risque de dépassement mémoire (*buffer overflow*).
* **Appels système dans `main()` :**
  1. `socket(AF_INET, SOCK_STREAM, 0)` : Crée la socket IPv4/TCP.
  2. `setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))` : Permet de relancer `server2` immédiatement après arrêt sans attendre l'expiration de l'état TCP `TIME_WAIT` (évite l'erreur *Address already in use*).
  3. `bind()` : Attache `sfd` à l'adresse locale `127.0.0.1` et au port `9002`.
  4. `listen(sfd, 5)` : Passe `sfd` en mode écoute avec une file d'attente de 5 connexions maximum.
  5. `accept(sfd, (struct sockaddr *)&cli, &clilen)` : Bloque jusqu'à l'arrivée d'un client, effectue la poignée de main TCP (*SYN / SYN-ACK / ACK*) et retourne `cfd`.
  6. Boucle `for (;;)` : Appelle `recv_line()`. Si `n <= 0` (client fermé via `Ctrl+C`) ou si `strcmp(buf, "/quit\n") == 0`, sort de la boucle (`break`), puis appelle `close(cfd)` et `close(sfd)`.

#### 2. `client2.c` (Client TCP conversationnel)
* **Variables et Buffers :**
  * `char buf[1024]` : Double usage — lit la saisie clavier via `fgets(buf, sizeof(buf), stdin)` (en conservant le `'\n'` final qui sert de délimiteur), puis sert de tampon de réception pour `recv_line()`.
* **Déroulement :**
  1. `socket(AF_INET, SOCK_STREAM, 0)` puis `connect(sfd, ...)` : Initie la connexion TCP vers l'IP et le port passés en arguments (`argv[1]`, `argv[2]`).
  2. Boucle `while (1)` : Lit une ligne au clavier, l'envoie intégralement via `send_all()`. Si la ligne est `"/quit\n"`, le client quitte la boucle immédiatement sans attendre de réponse du serveur, puis ferme la socket avec `close(sfd)`.

---

### B. Exercice 3 — Serveur TCP multithread (`server3.c`)

*Ce serveur réutilise `client2.c` sur le port `9003` et ajoute la concurrence via les threads POSIX.*

* **Variables globales partagées et synchronisation :**
  * `static int active_clients = 0` : Compteur global du nombre de clients connectés simultanément.
  * `static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER` : Verrou d'exclusion mutuelle protégeant `active_clients` contre les accès concurrents (*data races*) lorsque deux clients se connectent ou se déconnectent à la même microseconde.
* **Appels système et mécanismes spécifiques :**
  1. `signal(SIGPIPE, SIG_IGN)` : Empêche le noyau Linux de tuer tout le processus serveur si un client coupe brutalement sa connexion (`Ctrl+C`) au moment exact où un thread exécute `send()`.
  2. `int *pfd = malloc(sizeof(int))` : Dans la boucle `while (1)` de `main()`, chaque `cfd` retourné par `accept()` est copié dans un bloc mémoire alloué dynamiquement sur le tas avant d'être passé à `pthread_create()`. Cela évite qu'une nouvelle connexion n'écrase la variable `cfd` avant que le thread précédent ne l'ait lue.
  3. `pthread_create(&tid, NULL, handle_client, pfd)` : Démarre un fil d'exécution indépendant exécutant `handle_client()`.
  4. `pthread_detach(pthread_self())` : Indique au noyau de libérer automatiquement les ressources mémoire du thread dès qu'il se termine, sans avoir besoin d'appeler `pthread_join()`.
  5. `pthread_mutex_lock(&lock)` / `pthread_mutex_unlock(&lock)` : Encadre chaque incrémentation (`active_clients++`) à l'entrée du thread et chaque décrémentation (`active_clients--`) avant `close(cfd)`.

---

### C. Exercice 4 — Mesure de débit utile TCP (`server4.c` et `client4.c`)

#### 1. `server4.c` (Récepteur et calculateur de débit sur `127.0.0.1:9004`)
* **Buffers et chronométrage :**
  * `#define CHUNK_SIZE (64 * 1024)` : Taille de bloc de **64 Kio** (65 536 octets).
  * `char *buf = malloc(CHUNK_SIZE)` : Buffer alloué dynamiquement sur le tas pour absorber rapidement les paquets sans saturer la pile (*stack*).
  * `get_time_sec()` : Appelle `clock_gettime(CLOCK_MONOTONIC, &ts)` et convertit `tv_sec` + `tv_nsec` en `double` (secondes).
* **Logique de réception :**
  1. Dès le premier bloc reçu (`total_bytes == 0`), enregistre `start = get_time_sec()`.
  2. Accumule `total_bytes += (size_t)n` dans une boucle `recv(cfd, buf, CHUNK_SIZE, 0)` jusqu'à ce que `recv()` retourne `0` (fin de flux EOF).
  3. Calcule `end = get_time_sec()`, la durée `duration = end - start`, et le débit en mégabits par seconde : `((double)total_bytes * 8.0) / (duration * 1e6)`.
  4. Formate le bilan dans `char report[256]` via `snprintf()` et l'envoie au client avec `send()` avant `close(cfd)`.

#### 2. `client4.c` (Émetteur massif paramétrable)
* **Buffers et émission :**
  * `size_t total_to_send = (size_t)mib * 1024 * 1024` : Convertit l'argument `argv[3]` (en Mio) en nombre exact d'octets.
  * `char *buf = malloc(CHUNK_SIZE)` rempli avec `memset(buf, 'A', CHUNK_SIZE)`.
* **Appel système clé :**
  * **`shutdown(sfd, SHUT_WR)`** : Ferme **uniquement le canal d'écriture** (*half-close*). Le client envoie un segment TCP `FIN` au serveur pour lui indiquer qu'il n'y a plus de données à envoyer (déclenchant `recv() == 0` côté serveur), mais conserve son canal de lecture ouvert pour recevoir le `report` du serveur via `recv(sfd, report, ...)`.

---

### D. Exercice 5 — Service Echo UDP (`server5.c` et `client5.c`)

#### 1. `server5.c` (Serveur datagramme sans connexion sur `127.0.0.1:9005`)
* **Différences majeures avec TCP :**
  * `socket(AF_INET, SOCK_DGRAM, 0)` : Crée une socket UDP. Aucun appel à `listen()` ni `accept()` n'est effectué car UDP ne gère pas de session.
  * `recvfrom(sfd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&cli, &clilen)` : Lit un datagramme complet d'un seul coup et remplit automatiquement la structure `cli` avec l'IP et le port de l'expéditeur.
  * `sendto(sfd, buf, (size_t)n, 0, (struct sockaddr *)&cli, clilen)` : Renvoie immédiatement le datagramme à l'adresse contenue dans `cli`.

#### 2. `client5.c` (Client UDP avec timeout)
* **Gestion du timeout :**
  * `struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };`
  * `setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))` : Configure le noyau pour interrompre `recvfrom()` au bout de **2 secondes** si aucune réponse n'arrive (ex. serveur arrêté).
  * En cas d'expiration du délai, `recvfrom()` renvoie `-1` et positionne `errno` à `EAGAIN` ou `EWOULDBLOCK`, permettant d'afficher un message d'erreur propre au lieu de bloquer indéfiniment.

---

## 3. Fonctionnement détaillé de `test_tp.sh` et `menu.c`

### A. Le script Bash `test_tp.sh`
1. **Vérification préalable :** Boucle sur la liste des 7 binaires (`server2`, `client2`, `server3`, `server4`, `client4`, `server5`, `client5`) avec `[ ! -x "./$bin" ]` pour s'assurer qu'ils sont tous compilés et exécutables.
2. **Nettoyage garanti (`trap cleanup EXIT`) :** Enregistre une fonction `cleanup` qui tue automatiquement le serveur et `tcpdump` à la fin du script ou en cas d'interruption (`Ctrl+C`), évitant de laisser des ports occupés.
3. **Automatisation des entrées (`printf ... | ./client2`) :** Injecte directement les messages de test et la commande `/quit` dans l'entrée standard (`stdin`) des clients. Pour l'exercice 3, lance 3 clients en parallèle (`&`) avec de légers décalages (`sleep 0.2`, `0.4`, `0.6`) et synchronise leur fin avec `wait`.

### B. L'orchestrateur C `menu.c`
1. **`compiler_exercices()` :** Exécute les 7 commandes `gcc -std=c11 -Wall -Wextra ...` via `system()` au démarrage. Si une compilation échoue (`!= 0`), le programme s'arrête immédiatement.
2. **Authentification `sudo -v` au premier plan :** Valide le mot de passe administrateur avant le lancement de `tcpdump` en arrière-plan pour éviter tout blocage du clavier ou conflit d'affichage.
3. **Gestion multi-processus (`fork()` + `execl()` + `waitpid()`) :**
   * `lancer_bg(cmd)` crée un processus fils avec `fork()` qui remplace son image mémoire par `/bin/sh -c cmd` via `execl()`.
   * `tcpdump` est lancé avec `-i lo` (boucle locale), `-U` (*packet-buffered*, écriture immédiate sur disque sans mise en mémoire tampon), `-s 0` (capture complète sans troncature) et `</dev/null >/dev/null 2>&1` (empêche `tcpdump` de modifier les paramètres du terminal).
   * `arreter_tcpdump()` envoie `SIGINT` (`sudo -n pkill -INT -x tcpdump`) pour forcer la fermeture propre du fichier `.pcap`, et `system("stty sane")` restaure l'alignement du terminal avant d'afficher le contenu ASCII (`tcpdump -r ... -nn -A`) et de lancer **Wireshark**.