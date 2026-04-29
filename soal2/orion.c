#include "arena.h"
#include <signal.h>

int shmid, msgid, semid;
GameDatabase *db;

void handle_shutdown(int sig) {
    printf("\n[Orion] Shutting down. Saving universe...\n");
    FILE *fp = fopen("eterion.dat", "wb"); 
    if (fp != NULL) {
        fwrite(db, sizeof(GameDatabase), 1, fp);
        fclose(fp);
        printf("[Orion] Universe saved successfully to eterion.dat!\n");
    }
    shmdt(db);
    exit(0);
}

int main() {
    signal(SIGINT, handle_shutdown);

    shmid = shmget(SHM_KEY, sizeof(GameDatabase), IPC_CREAT | 0666);
    db = (GameDatabase *)shmat(shmid, NULL, 0);

    FILE *fp = fopen("eterion.dat", "rb"); 
    if (fp != NULL) {
        fread(db, sizeof(GameDatabase), 1, fp);
        fclose(fp);
        for(int i = 0; i < db->total_users; i++) {
            db->users[i].is_active = 0;
            db->users[i].status = 0;
        }
        printf("Orion is ready (PID: %d). DB Loaded: %d warriors.\n", getpid(), db->total_users);
    } else {
        db->total_users = 0; 
        for(int i=0; i<MAX_BATTLES; i++) db->arenas[i].active = 0;
        printf("Orion is ready (PID: %d). New universe started.\n", getpid());
    }

    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    semctl(semid, 0, SETVAL, 1); 

    GameMessage msg;
    while (1) {
        if (msgrcv(msgid, &msg, sizeof(GameMessage) - sizeof(long), 1, 0) > 0) {
            GameMessage reply = msg;
            reply.mtype = msg.sender_pid; 

            sem_lock(semid);
            if (msg.command == 1) { // REGISTER
                int exists = 0;
                for (int i = 0; i < db->total_users; i++) {
                    if (strcmp(db->users[i].username, msg.data1) == 0) { exists = 1; break; }
                }
                if (exists) reply.response = 0; 
                else {
                    strcpy(db->users[db->total_users].username, msg.data1);
                    strcpy(db->users[db->total_users].password, msg.data2);
                    db->users[db->total_users].gold = 150;     
                    db->users[db->total_users].lvl = 1;        
                    db->users[db->total_users].xp = 0;         
                    db->users[db->total_users].highest_dmg_weapon = 0;
                    db->users[db->total_users].history_count = 0;
                    db->users[db->total_users].is_active = 0;
                    db->users[db->total_users].status = 0;
                    db->total_users++;
                    reply.response = 1; 
                }
            } 
            else if (msg.command == 2) { // LOGIN
                int valid = 0;
                for (int i = 0; i < db->total_users; i++) {
                    if (strcmp(db->users[i].username, msg.data1) == 0 && strcmp(db->users[i].password, msg.data2) == 0) {
                        if (db->users[i].is_active == 0) {
                            valid = 1; db->users[i].is_active = 1; db->users[i].status = 0;
                        }
                        break;
                    }
                }
                reply.response = valid ? 1 : 0;
            }
            else if (msg.command == 4) { // LOGOUT
                for (int i = 0; i < db->total_users; i++) {
                    if (strcmp(db->users[i].username, msg.data1) == 0) {
                        db->users[i].is_active = 0; db->users[i].status = 0; break;
                    }
                }
                reply.response = 1;
            }
            sem_unlock(semid);
            msgsnd(msgid, &reply, sizeof(GameMessage) - sizeof(long), 0);
        }
    }
    return 0;
}
