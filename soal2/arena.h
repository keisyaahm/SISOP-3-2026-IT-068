#ifndef ARENA_H
#define ARENA_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <termios.h>

#define SHM_KEY 0x00001234
#define MSG_KEY 0x00005678
#define SEM_KEY 0x00009012

#define MAX_USERS 50
#define MAX_BATTLES 25

// Struktur Riwayat (Point 10)
typedef struct {
    char time_str[20];
    char opponent[50];
    char res[10]; // WIN / LOSS
    int xp_gained;
} MatchHistory;

// Struktur Data Pemain (Point 5 & 8)
typedef struct {
    char username[50];
    char password[50];
    int gold;
    int xp;
    int lvl;
    int highest_dmg_weapon; 
    int is_active;
    int status; // 0: Idle, 1: Matchmaking, 2: In Battle
    
    MatchHistory history[50];
    int history_count;
} User;

// Struktur Arena Pertempuran Real-time (Point 6 & 7)
typedef struct {
    int active;
    int is_bot; // 1 jika lawan adalah monster
    char p1[50]; char p2[50];
    int p1_hp; int p2_hp;
    int p1_max; int p2_max;
    char logs[5][100]; // 5 Log teratas
} BattleArena;

// Struktur Shared Memory Utama (Database)
typedef struct {
    User users[MAX_USERS];
    int total_users;
    BattleArena arenas[MAX_BATTLES];
} GameDatabase;

// Struktur Pesan untuk Message Queue
typedef struct {
    long mtype;       
    int sender_pid;
    int command;      // 1: Reg, 2: Login, 4: Logout
    char data1[50];   
    char data2[50];   
    int response;     
} GameMessage;

// Operasi Semaphore (Pengganti Mutex untuk IPC)
void sem_lock(int semid) {
    struct sembuf p = {0, -1, SEM_UNDO};
    semop(semid, &p, 1);
}
void sem_unlock(int semid) {
    struct sembuf v = {0, 1, SEM_UNDO};
    semop(semid, &v, 1);
}

#endif
