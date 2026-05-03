#include "arena.h"
#include <signal.h>

int msgid, shmid, semid;
GameDatabase *db;
char my_username[50];
int my_id_idx = -1;
int current_arena_idx = -1;
int in_battle = 0;

// Utilitas ambil jam
void get_time_string(char* buf) {
    time_t t = time(NULL); struct tm *tm = localtime(&t);
    sprintf(buf, "%02d:%02d", tm->tm_hour, tm->tm_min);
}

// Tambah log battle
void add_combat_log(int a_idx, char* msg) {
    for(int i=4; i>0; i--) strcpy(db->arenas[a_idx].logs[i], db->arenas[a_idx].logs[i-1]);
    strcpy(db->arenas[a_idx].logs[0], msg);
}

// Thread Khusus Render UI Battle (ANTI KEDAP-KEDIP)
void *battle_display_thread(void *arg) {
    printf("\033[H\033[J");
    while(in_battle) {
        printf("\033[H");
        printf("=== ARENA ===                                      \n");
        printf("%-15s HP: %-5d / %-5d           \n",
               db->arenas[current_arena_idx].p1,
               db->arenas[current_arena_idx].p1_hp,
               db->arenas[current_arena_idx].p1_max);
        printf("       VS                                          \n");
        printf("%-15s HP: %-5d / %-5d           \n\n",
               db->arenas[current_arena_idx].p2,
               db->arenas[current_arena_idx].p2_hp,
               db->arenas[current_arena_idx].p2_max);

        // Tampilkan info senjata yang dipakai
        int wpn = db->users[my_id_idx].highest_dmg_weapon;
        char wpn_name[20];
        if      (wpn >= 150) strcpy(wpn_name, "God Slayer");
        else if (wpn >= 60)  strcpy(wpn_name, "Demon Blade");
        else if (wpn >= 30)  strcpy(wpn_name, "Steel Axe");
        else if (wpn >= 15)  strcpy(wpn_name, "Iron Sword");
        else if (wpn >= 5)   strcpy(wpn_name, "Wood Sword");
        else                 strcpy(wpn_name, "None");

        printf("Weapon: %-15s | DMG Bonus: +%-3d           \n\n", wpn_name, wpn);

        printf("Combat Log:                                        \n");
        for(int i=0; i<5; i++) {
            if(strlen(db->arenas[current_arena_idx].logs[i]) > 0)
                printf("> %-50s\n", db->arenas[current_arena_idx].logs[i]);
            else
                printf("                                                    \n");
        }
        printf("\nCD: Atk(1s) | Ult(Ready if weapon>0)               \n");
        printf("Press 'a' to Attack, 'u' to Ultimate               \n");
        fflush(stdout);
        usleep(200000);
    }
    return NULL;
}

void process_battle_end(int is_win, char* opp_name) {
    sem_lock(semid);
    int gain_xp = is_win ? 50 : 15;
    int gain_gold = is_win ? 120 : 30;

    db->users[my_id_idx].xp += gain_xp;
    db->users[my_id_idx].gold += gain_gold;
    db->users[my_id_idx].lvl = 1 + (db->users[my_id_idx].xp / 100);

    int h_idx = db->users[my_id_idx].history_count;
    get_time_string(db->users[my_id_idx].history[h_idx].time_str);
    strcpy(db->users[my_id_idx].history[h_idx].opponent, opp_name);
    strcpy(db->users[my_id_idx].history[h_idx].res, is_win ? "WIN" : "LOSS");
    db->users[my_id_idx].history[h_idx].xp_gained = gain_xp;
    db->users[my_id_idx].history_count++;

    db->users[my_id_idx].status = 0; 
    db->arenas[current_arena_idx].active = 0; 
    sem_unlock(semid);

    printf("\033[H\033[J\n\n=== %s ===\nBattle ended. Press [ENTER] to continue...\n", is_win ? "VICTORY" : "DEFEAT");

    tcflush(STDIN_FILENO, TCIFLUSH);
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void matchmake_and_battle() {
    sem_lock(semid);
    db->users[my_id_idx].status = 1; 
    sem_unlock(semid);

    int opponent_found = 0;
    int is_p1 = 0;
    char opp_name[50] = "Wild Beast (Bot)";

    printf("\033[H\033[JSearching for an opponent...\n");

    for(int timer=1; timer<=35; timer++) {
        printf("\rSearching for an opponent... [%d s] ", timer); fflush(stdout);

        sem_lock(semid);
        if (db->users[my_id_idx].status == 2) {
            for(int i=0; i<MAX_BATTLES; i++) {
                if(db->arenas[i].active && strcmp(db->arenas[i].p2, my_username) == 0) {
                    current_arena_idx = i; 
                    strcpy(opp_name, db->arenas[i].p1); 
                    opponent_found = 1; is_p1 = 0; break;
                }
            }
            sem_unlock(semid);
            if (opponent_found) break;
        }

        if (!opponent_found) {
            for(int i=0; i<db->total_users; i++) {
                if(i != my_id_idx && db->users[i].status == 1 && db->users[i].is_active) {
                    for(int a=0; a<MAX_BATTLES; a++) {
                        if(!db->arenas[a].active) {
                            current_arena_idx = a; 
                            db->arenas[a].active = 1; db->arenas[a].is_bot = 0;
                            strcpy(db->arenas[a].p1, my_username); 
                            strcpy(db->arenas[a].p2, db->users[i].username);
                            db->arenas[a].p1_max = 100 + (db->users[my_id_idx].xp / 10);
                            db->arenas[a].p2_max = 100 + (db->users[i].xp / 10);
                            db->arenas[a].p1_hp = db->arenas[a].p1_max; db->arenas[a].p2_hp = db->arenas[a].p2_max;
                            for(int l=0; l<5; l++) strcpy(db->arenas[a].logs[l], "");
                            
                            db->users[my_id_idx].status = 2; db->users[i].status = 2;
                            opponent_found = 1; is_p1 = 1; 
                            strcpy(opp_name, db->users[i].username);
                            break;
                        }
                    }
                    break;
                }
            }
        }
        sem_unlock(semid);
        if(opponent_found) break;
        sleep(1);
    }

    if(!opponent_found) {
        sem_lock(semid);
        db->users[my_id_idx].status = 2; is_p1 = 1; 
        for(int a=0; a<MAX_BATTLES; a++) {
            if(!db->arenas[a].active) {
                current_arena_idx = a; 
                db->arenas[a].active = 1; db->arenas[a].is_bot = 1;
                strcpy(db->arenas[a].p1, my_username); strcpy(db->arenas[a].p2, opp_name);
                db->arenas[a].p1_max = 100 + (db->users[my_id_idx].xp / 10);
                db->arenas[a].p2_max = 100;
                db->arenas[a].p1_hp = db->arenas[a].p1_max; db->arenas[a].p2_hp = db->arenas[a].p2_max;
                for(int l=0; l<5; l++) strcpy(db->arenas[a].logs[l], "");
                break;
            }
        }
        sem_unlock(semid);
    }

    usleep(500000);

// KUNCI TERMINAL — VMIN=1 agar getchar() BLOCK sampai ada input
struct termios oldt, newt;
tcgetattr(STDIN_FILENO, &oldt);
newt = oldt;
newt.c_lflag &= ~(ICANON | ECHO); // matikan canonical + echo
newt.c_cc[VMIN] = 1;  // ← tunggu minimal 1 karakter
newt.c_cc[VTIME] = 0;
tcsetattr(STDIN_FILENO, TCSANOW, &newt);

// Hitung sekali sebelum battle dimulai
sem_lock(semid);
int current_dmg = 10 + (db->users[my_id_idx].xp / 50) + db->users[my_id_idx].highest_dmg_weapon;
sem_unlock(semid);

in_battle = 1;
pthread_t disp_tid;
pthread_create(&disp_tid, NULL, battle_display_thread, NULL);

while(in_battle) {
    sem_lock(semid);
    int m_hp = is_p1 ? db->arenas[current_arena_idx].p1_hp : db->arenas[current_arena_idx].p2_hp;
    int e_hp = is_p1 ? db->arenas[current_arena_idx].p2_hp : db->arenas[current_arena_idx].p1_hp;
    sem_unlock(semid);

    if (m_hp <= 0 || e_hp <= 0) break;

    int key = getchar();
    if (key == 'a' || key == 'u') {
        sem_lock(semid);
        // current_dmg sudah ada, tidak perlu hitung ulang
        int dmg_dealt = current_dmg;

        if (key == 'u') {
            if (db->users[my_id_idx].highest_dmg_weapon > 0) dmg_dealt = current_dmg * 3;
            else dmg_dealt = 0;
        }

        if (dmg_dealt > 0) {
            if (is_p1) db->arenas[current_arena_idx].p2_hp -= dmg_dealt;
            else       db->arenas[current_arena_idx].p1_hp -= dmg_dealt;

            char log_msg[100];
            sprintf(log_msg, "%s hit for %d damage!", my_username, dmg_dealt);
            add_combat_log(current_arena_idx, log_msg);
        }
        sem_unlock(semid);

        if (db->arenas[current_arena_idx].is_bot && is_p1) {
            sem_lock(semid);
            db->arenas[current_arena_idx].p1_hp -= 15;
            char blog[100];
            sprintf(blog, "Wild Beast hit for 15 damage!");
            add_combat_log(current_arena_idx, blog);
            sem_unlock(semid);
        }

        sleep(1); // cooldown
    }
    // tidak perlu else + usleep karena getchar() sudah blocking
}

tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // kembalikan terminal

    in_battle = 0; 
    pthread_join(disp_tid, NULL);

    sem_lock(semid);
    int my_final_hp = is_p1 ? db->arenas[current_arena_idx].p1_hp : db->arenas[current_arena_idx].p2_hp;
    sem_unlock(semid);

    process_battle_end(my_final_hp > 0, opp_name);
}

// Fungsi minta izin Server
int send_request(int cmd, char* d1, char* d2) {
    GameMessage msg; msg.mtype = 1; msg.sender_pid = getpid(); msg.command = cmd;
    strcpy(msg.data1, d1); strcpy(msg.data2, d2);
    msgsnd(msgid, &msg, sizeof(GameMessage)-sizeof(long), 0);
    GameMessage rep; msgrcv(msgid, &rep, sizeof(GameMessage)-sizeof(long), getpid(), 0);
    return rep.response;
}

void handle_client_sigint(int sig) {
    if (my_id_idx != -1) send_request(4, my_username, "");
    exit(0);
}

int main() {
    signal(SIGINT, handle_client_sigint);
    shmid = shmget(SHM_KEY, sizeof(GameDatabase), 0666);
    msgid = msgget(MSG_KEY, 0666);
    semid = semget(SEM_KEY, 1, 0666);

    if (shmid < 0 || msgid < 0) { printf("Orion are you there?\n"); return 1; }
    db = (GameDatabase *)shmat(shmid, NULL, 0);

    int choice; char u[50], p[50];

    while(1) {
        printf("\n=== ETERION ===\n1. Register\n2. Login\n3. Exit\nChoice: ");
        scanf("%d", &choice);

        if (choice == 1) {
            printf("Username: "); scanf("%s", u); printf("Password: "); scanf("%s", p);
            if(send_request(1, u, p)) printf("Account created!\n"); else printf("Username dipakai!\n");
        } else if (choice == 2) {
            printf("Username: "); scanf("%s", u); printf("Password: "); scanf("%s", p);
            if(send_request(2, u, p)) {
                strcpy(my_username, u);

                for(int i=0; i<db->total_users; i++) {
                    if(strcmp(db->users[i].username, my_username)==0) { my_id_idx = i; break; }
                }

                int menu;
                while(1) {
                    sem_lock(semid);
                    int lvl = db->users[my_id_idx].lvl; int gold = db->users[my_id_idx].gold; int xp = db->users[my_id_idx].xp;
                    sem_unlock(semid);

                    printf("\n┌────────────────────────────────────────┐\n");
                    printf("│                PROFILE                 │\n");
                    printf("├────────────────────────────────────────┤\n");
                    printf("│ Name : %-15s Lvl : %-8d│\n", my_username, lvl);
                    printf("│ Gold : %-15d XP  : %-8d│\n", gold, xp);
                    printf("└────────────────────────────────────────┘\n");
                    printf("1. Battle\n2. Armory\n3. History\n4. Logout\nChoice: ");
                    scanf("%d", &menu);

                    if (menu == 1) matchmake_and_battle();
                    else if (menu == 2) {
                        printf("\n== ARMORY ==\nGold: %d\n", db->users[my_id_idx].gold);
                        printf("1. Wood Sword  (100G,  +5 Dmg)\n");
                        printf("2. Iron Sword  (300G,  +15 Dmg)\n");
                        printf("3. Steel Axe   (600G,  +30 Dmg)\n");
                        printf("4. Demon Blade (1500G, +60 Dmg)\n");
                        printf("5. God Slayer  (5000G, +150 Dmg)\n");
                        printf("0. Back\nChoice: ");
                        int buy; scanf("%d", &buy);
                        sem_lock(semid);
                        int *g = &db->users[my_id_idx].gold;
                        int *w = &db->users[my_id_idx].highest_dmg_weapon;
                        if      (buy==1 && *g>=100)  { *g-=100;  if(5   > *w) *w=5;   printf("Beli Wood Sword sukses!\n"); }
                        else if (buy==2 && *g>=300)  { *g-=300;  if(15  > *w) *w=15;  printf("Beli Iron Sword sukses!\n"); }
                        else if (buy==3 && *g>=600)  { *g-=600;  if(30  > *w) *w=30;  printf("Beli Steel Axe sukses!\n"); }
                        else if (buy==4 && *g>=1500) { *g-=1500; if(60  > *w) *w=60;  printf("Beli Demon Blade sukses!\n"); }
                        else if (buy==5 && *g>=5000) { *g-=5000; if(150 > *w) *w=150; printf("Beli God Slayer sukses!\n"); }
                        else if (buy!=0) printf("Gold tidak cukup atau pilihan salah!\n");
                        sem_unlock(semid);
                    }
                    else if (menu == 3) {
                        printf("\n--- MATCH HISTORY ---\nTime\tOpponent\tRes\tXP\n");
                        sem_lock(semid);
                        for(int i=0; i<db->users[my_id_idx].history_count; i++) {
                            MatchHistory h = db->users[my_id_idx].history[i];
                            printf("%s\t%s\t%s\t+%d XP\n", h.time_str, h.opponent, h.res, h.xp_gained);
                        }
                        sem_unlock(semid);
                        while(getchar() != '\n');
                        printf("Press [ENTER] to return...");
                        getchar();
                    }
                    else if (menu == 4) { send_request(4, my_username, ""); break; }
                }
            } else printf("Login gagal!\n");
        } else break;
    }
    return 0;
}
