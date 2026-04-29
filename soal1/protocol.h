#ifndef PROTOCOL_H
#define PROTOCOL_H

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
MSG_RPC_RES //server bls req admin
} MsgType;

//Struktur paket data yg bakal dikirim lewat Socket
typedef struct {
MsgType type;
char sender[50];
char content[BUFFER_SIZE]; //simpan isi cht
} Packet;

//Fungsi bantuan yang bisa dipakai bersama yg include file ini
void get_current_time(char *buffer);

#endif
