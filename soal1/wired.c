#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "protocol.h"

//struktur data client
typedef struct {
int socket; //no colokan
char name[50]; //nama acc
int is_admin; //1 Admin 0 Biasa
int is_active; //1 masih aktif, 0 disconnect
} ClientInfo;

ClientInfo clients[MAX_CLIENTS]; //kapasitas array buku tamu 100 orang

// MUTEX: cegah race condition
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex array clients
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex file history.log

time_t server_start_time; //fitur uptime, kapan server pertma

//catat ke file history.log
void write_log(const char *role, const char *action) {
    pthread_mutex_lock(&log_mutex); //LOCK kalau ada thread lain mau nulis log, dia harus ngantri di sini
    
    FILE *f = fopen("history.log", "a"); //buka file dengan mode "a" (Append/Tambah di bawah) bukan ditimpa
    if (f) {
        char time_str[30];
        get_current_time(time_str); //fungsi waktu di protokol c
        fprintf(f, "[%s] [%s] [%s]\n", time_str, role, action); //cetak format log ke dalam file
        fclose(f); //tutup file+tersimpan di harddisk
    }
    
    pthread_mutex_unlock(&log_mutex); //OPEN thread lain yg ngantri sekarang boleh nulis
}

//fungsi meneruskan chat
void broadcast_message(Packet *pkt, int sender_socket) {
    pthread_mutex_lock(&clients_mutex); //LOCK data user agar tidak ada yg tiba-tiba disconnect saat looping
    
    //Looping dari 0 sampai 99
    for (int i = 0; i < MAX_CLIENTS; i++) {
        //ke semua client aktif, KECUALI si pengirim
        if (clients[i].is_active && clients[i].socket != sender_socket) {
            send(clients[i].socket, pkt, sizeof(Packet), 0);
        }
    }
    
    pthread_mutex_unlock(&clients_mutex); //OPEN LOCK
}

//Thread khusus untuk menangani 1 Client
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    Packet pkt;
    int client_index = -1; //client dmn

    // FASE 1: Menunggu Login
    while (recv(client_socket, &pkt, sizeof(Packet), 0) > 0) {
        if (pkt.type == MSG_LOGIN) { //kalo client mau login
            pthread_mutex_lock(&clients_mutex);
            int name_exists = 0;

            //Cek apakah nama sudah ada
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].is_active && strcmp(clients[i].name, pkt.sender) == 0) {
                    name_exists = 1; break; // Nama ketemu! Set flag ke 1, lalu keluar dari loop.
                }
            }

            if (name_exists) {
                // Tolak login
                Packet res = {MSG_ERROR, "System", "The identity is already synchronized in The Wired.\n"};
                send(client_socket, &res, sizeof(Packet), 0);
                pthread_mutex_unlock(&clients_mutex); //lepas gembok
                close(client_socket);
                return NULL;
            }

            //NAMA DITERIMA Cari slot kosong di array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!clients[i].is_active) {
                    clients[i].socket = client_socket; //catat socket
                    strcpy(clients[i].name, pkt.sender); //catat nama
                    //namanya "The Knights", dia admin (1). Jika bukan, dia user biasa (0).
                    clients[i].is_admin = (strcmp(pkt.sender, "The Knights") == 0) ? 1 : 0;
                    clients[i].is_active = 1;                 // Tandai dia online.
                    client_index = i;                         // Ingat nomor kursinya.
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);

	    // [REVISI] Jika sukses, kirim sambutan HANYA JIKA BUKAN ADMIN
            if (!clients[client_index].is_admin) {
                char welcome_msg[150];
                sprintf(welcome_msg, "--- Welcome to The Wired, %s ---\n", pkt.sender);
                Packet res = {MSG_CHAT, "System", ""};
                strcpy(res.content, welcome_msg);
                send(client_socket, &res, sizeof(Packet), 0);
            }

            // Log dan Broadcast kalau ada user masuk
            char log_msg[100];
            if (clients[client_index].is_admin) {
                sprintf(log_msg, "User 'The Knights' connected");
                write_log("System", log_msg);
            } else {
                sprintf(log_msg, "User '%s' connected", pkt.sender);
                write_log("System", log_msg);
            }

            break; // Selesai fase login, masuk ke fase Chat
        }
    }

    // FASE 2: Mendengarkan Chat/Command dari Client ini
    while (recv(client_socket, &pkt, sizeof(Packet), 0) > 0) {
        if (pkt.type == MSG_EXIT) {
            break; // Keluar loop untuk disconnect
        } 
        else if (pkt.type == MSG_CHAT) {
            //user send pesan, tulis Log chatnya
            char log_msg[2048];
            sprintf(log_msg, "[%s]: %s", pkt.sender, pkt.content);
            write_log("User", log_msg);
            
            // Sebarkan ke yang lain
            broadcast_message(&pkt, client_socket);
        }
        else if (pkt.type == MSG_RPC_REQ && clients[client_index].is_admin) {
            // RPC Command untuk Admin
            write_log("Admin", pkt.content); // Misal: [RPC_GET_USERS]
            Packet res = {MSG_RPC_RES, "System", ""};
            
            if (strcmp(pkt.content, "RPC_GET_USERS") == 0) {
                int count = 0;
                pthread_mutex_lock(&clients_mutex);
                for(int i=0; i<MAX_CLIENTS; i++) {
                    // Cek aktif dan BUKAN admin
                    if(clients[i].is_active && !clients[i].is_admin) count++;
                }
                pthread_mutex_unlock(&clients_mutex);
                sprintf(res.content, "Active NAVI Units: %d\n", count);
                send(client_socket, &res, sizeof(Packet), 0);
            } 
            else if (strcmp(pkt.content, "RPC_GET_UPTIME") == 0) {
                time_t now = time(NULL);
                int diff = (int)(now - server_start_time);
                sprintf(res.content, "Server Uptime: %d seconds\n", diff);
                send(client_socket, &res, sizeof(Packet), 0);
            }
            else if (strcmp(pkt.content, "RPC_SHUTDOWN") == 0) {
                write_log("System", "EMERGENCY SHUTDOWN INITIATED");
                printf("[System] Emergency Shutdown requested by Admin.\n");
                exit(0); // Matikan server seketika
            }
        }
    }

    // FASE 3: Disconnect Handling
    if (client_index != -1) { //pastiin dia sempet login
        char log_msg[100];
        sprintf(log_msg, "User '%s' disconnected", clients[client_index].name);
        write_log("System", log_msg);

        pthread_mutex_lock(&clients_mutex);
        clients[client_index].is_active = 0; //tandai kursi kosong lg biar bs dipakai lagi
        pthread_mutex_unlock(&clients_mutex);
    }
    close(client_socket);
    return NULL; //matikan tread
}

//Handle Ctrl+C di server
void handle_sigint(int sig) {
    write_log("System", "SERVER OFFLINE");
    printf("\n[System] Shutting down The Wired...\n");
    exit(0);
}

//MAIN SERVER
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;

    //bersihkan file log setiap server restart
    FILE *f = fopen("history.log", "w");
    if(f) fclose(f);

    server_start_time = time(NULL); //catat detik kapan server diidupin
    write_log("System", "SERVER ONLINE");
    signal(SIGINT, handle_sigint); //control c server bs mati sendiri

    //1. Membuat socket TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed"); exit(1);
    }

    //2. SO_REUSEADDR agar port tidak nyangkut saat server direstart
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    //3. Konfigurasi Alamat
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    //4. BIND: Mematenkan colokan socket tadi dengan alamat dan port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed"); exit(1);
    }

    //5. LISTEN: Server mulai pasang telinga, siap menerima antrean hingga 10 orang di ruang tunggu koneksi OS
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed"); exit(1);
    }

    printf("=== THE WIRED IS ONLINE ===\n");

    //6. ACCEPT LOOP: Loop selamanya menerima client baru
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            continue;
        }

        // Buat thread baru per client yang masuk
        pthread_t tid;
        int *client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = new_socket;
        pthread_create(&tid, NULL, handle_client, (void *)client_sock_ptr);
        pthread_detach(tid); // Agar resource memory thread otomatis bersih saat selesai
    }

    return 0;
}
