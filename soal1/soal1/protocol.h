#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <time.h>
#include <stdio.h>

#define PORT 8080
#define IP "127.0.0.1"
#define MAX_CLIENTS 100 //batas max orang yg online bersama
#define BUFFER_SIZE 1024 //max karakter per psn

//Tipe pesan
typedef enum {
MSG_LOGIN, //pertama kirim
MSG_CHAT, //psn biasa
MSG_EXIT, //mau keluar
MSG_RPC_REQ, //req dari admin ke server
MSG_RPC_RES, //server bls req admin
MSG_ERROR
} MsgType;

//Struktur paket data yg bakal dikirim lewat Socket
typedef struct {
MsgType type;
char sender[50];
char content[BUFFER_SIZE]; //simpan isi cht
} Packet;

// Fungsi bantuan (Langsung implementasi di header dengan static inline)
static inline void get_current_time(char *buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
}

#endif
