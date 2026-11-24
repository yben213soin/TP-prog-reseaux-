#!/bin/bash

# Vérification que tous les exécutables sont compilés
for bin in server2 client2 server3 server4 client4 server5 client5; do
    if [[ ! -x "./$bin" ]]; then
        echo "[Erreur] Exécutable ./$bin manquant. Compile tous tes fichiers avant de lancer le script."
        exit 1
    fi
done

# Fonction de nettoyage automatique à la fin ou en cas de Ctrl+C
cleanup() {
    if [[ -n "$SERVER_PID" ]]; then kill "$SERVER_PID" 2>/dev/null; fi
    if [[ -n "$TCPDUMP_PID" ]]; then sudo kill "$TCPDUMP_PID" 2>/dev/null; fi
}
trap cleanup EXIT

echo "=================================================="
echo "   MENU DE TEST ET CAPTURE WIRESHARK (TP RÉSEAU)  "
echo "=================================================="
echo "2) Exercice 2 : Chat TCP séquentiel (port 9002)"
echo "3) Exercice 3 : Serveur TCP multithread - 3 clients (port 9003)"
echo "4) Exercice 4 : Mesure de débit TCP (port 9004)"
echo "5) Exercice 5 : Client/Serveur Echo UDP (port 9005)"
echo "--------------------------------------------------"
read -p "Choisis l'exercice à lancer (2, 3, 4 ou 5) : " CHOIX

case "$CHOIX" in
    2)
        PORT=9002
        PROTO="tcp"
        PCAP="capture_exo2.pcap"
        ./server2 &
        SERVER_PID=$!
        sleep 0.5

        echo "[Capture] Démarrage de la capture sur lo (port $PORT)..."
        sudo tcpdump -i lo "$PROTO" port "$PORT" -w "$PCAP" 2>/dev/null &
        TCPDUMP_PID=$!
        sleep 1

        echo "[Test Exo 2] Envoi de 3 messages puis /quit..."
        printf "Message 1\nMessage 2\nMessage 3\n/quit\n" | ./client2 127.0.0.1 "$PORT"
        ;;

    3)
        PORT=9003
        PROTO="tcp"
        PCAP="capture_exo3.pcap"
        ./server3 &
        SERVER_PID=$!
        sleep 0.5

        echo "[Capture] Démarrage de la capture sur lo (port $PORT)..."
        sudo tcpdump -i lo "$PROTO" port "$PORT" -w "$PCAP" 2>/dev/null &
        TCPDUMP_PID=$!
        sleep 1

        echo "[Test Exo 3] Lancement de 3 clients simultanés..."
        (sleep 0.2; printf "Bonjour depuis Client 1\n/quit\n") | ./client2 127.0.0.1 "$PORT" &
        P1=$!
        (sleep 0.4; printf "Salut depuis Client 2\n/quit\n") | ./client2 127.0.0.1 "$PORT" &
        P2=$!
        (sleep 0.6; printf "Hello depuis Client 3\n/quit\n") | ./client2 127.0.0.1 "$PORT" &
        P3=$!
        wait $P1 $P2 $P3
        ;;

    4)
        PORT=9004
        PROTO="tcp"
        PCAP="capture_exo4.pcap"
        read -p "Volume à transférer en Mio (ex: 1, 32 ou 128) : " VOL
        ./server4 &
        SERVER_PID=$!
        sleep 0.5

        echo "[Capture] Démarrage de la capture sur lo (port $PORT)..."
        sudo tcpdump -i lo "$PROTO" port "$PORT" -w "$PCAP" 2>/dev/null &
        TCPDUMP_PID=$!
        sleep 1

        echo "[Test Exo 4] Transfert de ${VOL} Mio..."
        ./client4 127.0.0.1 "$PORT" "$VOL"
        ;;

    5)
        PORT=9005
        PROTO="udp"
        PCAP="capture_exo5.pcap"
        ./server5 &
        SERVER_PID=$!
        sleep 0.5

        echo "[Capture] Démarrage de la capture sur lo (port $PORT)..."
        sudo tcpdump -i lo "$PROTO" port "$PORT" -w "$PCAP" 2>/dev/null &
        TCPDUMP_PID=$!
        sleep 1

        echo "[Test Exo 5] Envoi d'un datagramme UDP..."
        printf "Test datagramme UDP\n" | ./client5 127.0.0.1 "$PORT"
        ;;

    *)
        echo "Choix invalide. Relance le script et tape 2, 3, 4 ou 5."
        exit 1
        ;;
esac

sleep 1
sudo kill "$TCPDUMP_PID" 2>/dev/null
TCPDUMP_PID=""

echo ""
echo "=================================================="
echo "   APERÇU DU CONTENU DES DONNÉES CAPTURÉES         "
echo "=================================================="
# Affiche les données utiles en ASCII (-A) directement dans le terminal
tcpdump -r "$PCAP" -nn -A 2>/dev/null | head -n 45

echo ""
echo "[Wireshark] Fichier enregistré sous : $PCAP"
read -p "Ouvrir $PCAP dans Wireshark maintenant ? (o/n) : " OUVRIR
if [[ "$OUVRIR" == "o" || "$OUVRIR" == "O" ]]; then
    wireshark "$PCAP" &
fi