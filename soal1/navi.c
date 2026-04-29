#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "protocol.h"

int sock = 0;
char my_name[50];
int is_admin = 0;

// Menangkap sinyal Ctrl+C agar logout dengan rapi
void handle_sigint(int sig) {
    Packet pkt = {MSG_EXIT, "", ""};
    strcpy(pkt.sender, my_name);
    send(sock, &pkt, sizeof(Packet), 0);
    printf("\n[System] Disconnecting from The Wired...\n");
    close(sock);
    
    _exit(0);
}

// Thread Khusus Menerima Pesan Asinkron (dari Server)
void *receive_handler(void *arg) {
    Packet pkt;
    while (recv(sock, &pkt, sizeof(Packet), 0) > 0) {
        if (pkt.type == MSG_CHAT) {
            if (strcmp(pkt.sender, "System") == 0) {
                // Sapaan awal dari sistem
                printf("%s> ", pkt.content);
                fflush(stdout);
            } else {
                // Tampilan broadcast dari user lain
                printf("\n[%s]: %s\n> ", pkt.sender, pkt.content);
                fflush(stdout); 
            }
        } 
	else if (pkt.type == MSG_ERROR) {
            printf("[System] %s", pkt.content);
            fflush(stdout); // Paksa teks error tercetak ke layar saat ini juga
            
            _exit(0); // [REVISI] Tembak mati program seketika tanpa memicu Deadlock I/O
        }	

        else if (pkt.type == MSG_RPC_RES) {
            printf("\n%s\n> ", pkt.content);
            fflush(stdout);
        }
    }

    // [REVISI]
    // Kalau loop while di atas berhenti, artinya recv() = 0 (Koneksi dari Server Terputus!)
    printf("\n[System] Connection lost. The Wired has been shut down.\n");
    fflush(stdout);
    _exit(0);

    return NULL;
}

int main() {
    struct sockaddr_in serv_addr;
    Packet pkt;

    signal(SIGINT, handle_sigint);

    // Bikin socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n"); return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n"); return -1;
    }

    // Konek ke The Wired
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed. Server Offline.\n"); return -1;
    }

    // FASE LOGIN INPUT NAMA, krn blm login cht engga aktif
    printf("Enter your name: ");
    fgets(my_name, 50, stdin);
    my_name[strcspn(my_name, "\n")] = 0; // Hilangkan enter (\n) di akhir

    //OTENTIKASI ADMIN
    if (strcmp(my_name, "The Knights") == 0) {
        //minta pw
        char pass[50];
        printf("Enter Password: ");
        fgets(pass, 50, stdin);
        pass[strcspn(pass, "\n")] = 0;

        if (strcmp(pass, "protocol7") != 0) {
            printf("[System] Authentication Failed.\n");
            close(sock); return 0;
        }
        is_admin = 1;
        printf("\n[System] Authentication Successful. Granted Admin privileges.\n");
    }

    // Kirim Request Login
    pkt.type = MSG_LOGIN;
    strcpy(pkt.sender, my_name);
    send(sock, &pkt, sizeof(Packet), 0);

    //SETUP ASINKRON: Jalankan Thread Receiver agar bisa ngetik & baca barengan
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_handler, NULL);

    // MAIN LOOP: Loop Utama Main Thread
    char buffer[BUFFER_SIZE];
    while (1) {
        if (!is_admin) {
            // Mode Chat Normal (SUDAH TIDAK ADA printf("> ") DI SINI)
            fgets(buffer, BUFFER_SIZE, stdin);
            buffer[strcspn(buffer, "\n")] = 0;

            if (strcmp(buffer, "/exit") == 0) {
                handle_sigint(0); 
            } else if (strlen(buffer) > 0) {
                Packet chat_pkt = {MSG_CHAT, "", ""};
                strcpy(chat_pkt.sender, my_name);
                strcpy(chat_pkt.content, buffer);
                send(sock, &chat_pkt, sizeof(Packet), 0);
                
                // Print ulang prompt > SETELAH user ngirim chat
                printf("> ");
                fflush(stdout);
            }
        } else {

            // Mode Admin (The Knights Console)
            sleep(1); // Kasih nafas dikit biar UI menu nggak ketimpa pesan masuk
            printf("\n=== THE KNIGHTS CONSOLE ===\n");
            printf("1. Check Active Entities (Users)\n");
            printf("2. Check Server Uptime\n");
            printf("3. Execute Emergency Shutdown\n");
            printf("4. Disconnect\n");
            printf("Command >> ");

            int cmd;
            scanf("%d", &cmd);
            getchar(); // buang sisa enter

            Packet rpc_pkt = {MSG_RPC_REQ, "", ""};
            strcpy(rpc_pkt.sender, my_name);

		if (cmd == 1) strcpy(rpc_pkt.content, "RPC_GET_USERS");
		else if (cmd == 2) strcpy(rpc_pkt.content, "RPC_GET_UPTIME");
		else if (cmd == 3) strcpy(rpc_pkt.content, "RPC_SHUTDOWN");
		else if (cmd == 4) handle_sigint(0);
		else {
   		printf("[System] Invalid command.\n");
    		continue;
		}

		send(sock, &rpc_pkt, sizeof(Packet), 0);
        }
    }

    return 0;
}
