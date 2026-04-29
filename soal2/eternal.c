#include "arena.h"

int msgid, shmid, semid;
GameDatabase *db;
char my_username[50];
int my_id_idx = -1;
int current_arena_idx = -1;
int in_battle = 0;

// Ambil input keyboard tanpa enter (NON-BLOCKING)
int getch(void) {
    struct termios oldattr, newattr; int ch;
    tcgetattr(STDIN_FILENO, &oldattr);
    newattr = oldattr; 
    newattr.c_lflag &= ~(ICANON | ECHO);
    newattr.c_cc[VMIN] = 0;  // Jangan tunggu karakter
    newattr.c_cc[VTIME] = 1; // Timeout 0.1 detik
    tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
    return ch;
}

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

// Thread Khusus Render UI Battle Secara Realtime
void *battle_display_thread(void *arg) {
    while(in_battle) {
        printf("\033[H\033[J"); // Clear Screen Linux
        printf("=== ARENA ===\n");
        printf("%-15s HP: %d/%d\n", db->arenas[current_arena_idx].p1, db->arenas[current_arena_idx].p1_hp, db->arenas[current_arena_idx].p1_max);
        printf("       VS\n");
        printf("%-15s HP: %d/%d\n\n", db->arenas[current_arena_idx].p2, db->arenas[current_arena_idx].p2_hp, db->arenas[current_arena_idx].p2_max);
        printf("Combat Log:\n");
        for(int i=0; i<5; i++) {
            if(strlen(db->arenas[current_arena_idx].logs[i]) > 0) printf("> %s\n", db->arenas[current_arena_idx].logs[i]);
        }
        printf("\nCD: Atk(1s) | Ult(Ready if weapon>0)\nPress 'a' to Attack, 'u' to Ultimate\n");
        usleep(200000); // Refresh 5x sedetik
    }
    return NULL;
}

void process_battle_end(int is_win, char* opp_name) {
    sem_lock(semid);
    int gain_xp = is_win ? 50 : 15;
    int gain_gold = is_win ? 120 : 30;
    
    db->users[my_id_idx].xp += gain_xp;
    db->users[my_id_idx].gold += gain_gold;
    // Level naik setiap kelipatan 100 XP
    db->users[my_id_idx].lvl = 1 + (db->users[my_id_idx].xp / 100);
    
    int h_idx = db->users[my_id_idx].history_count;
    get_time_string(db->users[my_id_idx].history[h_idx].time_str);
    strcpy(db->users[my_id_idx].history[h_idx].opponent, opp_name);
    strcpy(db->users[my_id_idx].history[h_idx].res, is_win ? "WIN" : "LOSS");
    db->users[my_id_idx].history[h_idx].xp_gained = gain_xp;
    db->users[my_id_idx].history_count++;
    
    db->users[my_id_idx].status = 0; // Balik Idle
    db->arenas[current_arena_idx].active = 0; // Tutup arena
    sem_unlock(semid);

    printf("\033[H\033[J\n\n=== %s ===\nBattle ended. Press [ENTER] to continue...\n", is_win ? "VICTORY" : "DEFEAT");
    
    // Bersihkan buffer keyboard dari sisa spam
    tcflush(STDIN_FILENO, TCIFLUSH);
    int c; 
    while ((c = getchar()) != '\n' && c != EOF); 
}

void matchmake_and_battle() {
    sem_lock(semid);
    db->users[my_id_idx].status = 1; // 1 = Matchmaking
    sem_unlock(semid);

    int opponent_found = 0;
    int is_p1 = 0;
    char opp_name[50] = "Wild Beast (Bot)";

    printf("\033[H\033[JSearching for an opponent...\n");

    // Loop 35 Detik untuk mencari lawan sesama pemain
    for(int timer=1; timer<=35; timer++) {
        printf("\rSearching for an opponent... [%d s] ", timer); fflush(stdout);
        
        sem_lock(semid);
        // Cek kalau tiba-tiba aku ditarik orang ke arena (Aku jadi P2)
        if (db->users[my_id_idx].status == 2) {
            opponent_found = 1; is_p1 = 0;
            // Cari arenaku
            for(int i=0; i<MAX_BATTLES; i++) {
                if(db->arenas[i].active && strcmp(db->arenas[i].p2, my_username) == 0) {
                    current_arena_idx = i; strcpy(opp_name, db->arenas[i].p1); break;
                }
            }
            sem_unlock(semid); break;
        }

        // Aku mencoba nyari orang lain yang statusnya 1 (Matchmaking)
        for(int i=0; i<db->total_users; i++) {
            if(i != my_id_idx && db->users[i].status == 1 && db->users[i].is_active) {
                // Ketemu! Aku jadi P1, dia P2
                db->users[my_id_idx].status = 2; db->users[i].status = 2;
                opponent_found = 1; is_p1 = 1; strcpy(opp_name, db->users[i].username);
                
                // Siapkan Arena
                for(int a=0; a<MAX_BATTLES; a++) {
                    if(!db->arenas[a].active) {
                        current_arena_idx = a; db->arenas[a].active = 1; db->arenas[a].is_bot = 0;
                        strcpy(db->arenas[a].p1, my_username); strcpy(db->arenas[a].p2, opp_name);
                        db->arenas[a].p1_max = 100 + (db->users[my_id_idx].xp / 10);
                        db->arenas[a].p2_max = 100 + (db->users[i].xp / 10);
                        db->arenas[a].p1_hp = db->arenas[a].p1_max; db->arenas[a].p2_hp = db->arenas[a].p2_max;
                        for(int l=0; l<5; l++) strcpy(db->arenas[a].logs[l], "");
                        break;
                    }
                }
                break;
            }
        }
        sem_unlock(semid);
        if(opponent_found) break;
        sleep(1);
    }

    // Jika 35 dtk habis, lawan Bot
    if(!opponent_found) {
        sem_lock(semid);
        db->users[my_id_idx].status = 2; is_p1 = 1;
        for(int a=0; a<MAX_BATTLES; a++) {
            if(!db->arenas[a].active) {
                current_arena_idx = a; db->arenas[a].active = 1; db->arenas[a].is_bot = 1;
                strcpy(db->arenas[a].p1, my_username); strcpy(db->arenas[a].p2, opp_name);
                db->arenas[a].p1_max = 100 + (db->users[my_id_idx].xp / 10);
                db->arenas[a].p2_max = 100; // Darah Bot Tetap
                db->arenas[a].p1_hp = db->arenas[a].p1_max; db->arenas[a].p2_hp = db->arenas[a].p2_max;
                for(int l=0; l<5; l++) strcpy(db->arenas[a].logs[l], "");
                break;
            }
        }
        sem_unlock(semid);
    }

    // MASUK BATTLE ASINKRON
    in_battle = 1;
    pthread_t disp_tid;
    pthread_create(&disp_tid, NULL, battle_display_thread, NULL);

    int my_dmg = 10 + (db->users[my_id_idx].xp / 50) + db->users[my_id_idx].highest_dmg_weapon;

    // Main Loop Battle untuk menangkap ketikan user
    while(1) {
        sem_lock(semid);
        int m_hp = is_p1 ? db->arenas[current_arena_idx].p1_hp : db->arenas[current_arena_idx].p2_hp;
        int e_hp = is_p1 ? db->arenas[current_arena_idx].p2_hp : db->arenas[current_arena_idx].p1_hp;
        sem_unlock(semid);

        if (m_hp <= 0 || e_hp <= 0) break; // Pertarungan selesai

        char key = getch();
        if (key == 'a' || key == 'u') {
            sem_lock(semid);
            int dmg_dealt = my_dmg;
            if (key == 'u') {
                if (db->users[my_id_idx].highest_dmg_weapon > 0) dmg_dealt = my_dmg * 3;
                else dmg_dealt = 0; // Gagal ulti kalau ga punya senjata
            }

            if (dmg_dealt > 0) {
                if (is_p1) db->arenas[current_arena_idx].p2_hp -= dmg_dealt;
                else db->arenas[current_arena_idx].p1_hp -= dmg_dealt;

                char log_msg[100]; 
                sprintf(log_msg, "%s hit for %d damage!", my_username, dmg_dealt);
                add_combat_log(current_arena_idx, log_msg);
            }
            sem_unlock(semid);
            
            // Simulasi Serangan Balasan dari Bot
            if (db->arenas[current_arena_idx].is_bot && is_p1) {
                sem_lock(semid);
                db->arenas[current_arena_idx].p1_hp -= 15; // Dmg bot = 15
                char blog[100]; 
                sprintf(blog, "Wild Beast hit for 15 damage!");
                add_combat_log(current_arena_idx, blog);
                sem_unlock(semid);
            }

            sleep(1); // Cooldown 1 detik per serangan
	    tcflush(STDIN_FILENO, TCIFLUSH);
        }
    }

    in_battle = 0; // Matikan thread display
    pthread_join(disp_tid, NULL);

    // Hitung Menang/Kalah
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

int main() {
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
                
                // Cari index kita di DB
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
                        printf("\n== ARMORY ==\nGold: %d\n1. Wood Sword (100G, +5 Dmg)\n2. Iron Sword (300G, +15 Dmg)\n3. Demon Blade (1500G, +60 Dmg)\n0. Back\nChoice: ", gold);
                        int buy; scanf("%d", &buy);
                        sem_lock(semid);
                        if(buy==1 && db->users[my_id_idx].gold>=100) { db->users[my_id_idx].gold-=100; db->users[my_id_idx].highest_dmg_weapon = 5; printf("Beli Wood Sword sukses!\n"); }
                        else if(buy==2 && db->users[my_id_idx].gold>=300) { db->users[my_id_idx].gold-=300; db->users[my_id_idx].highest_dmg_weapon = 15; printf("Beli Iron Sword sukses!\n"); }
                        else if(buy==3 && db->users[my_id_idx].gold>=1500) { db->users[my_id_idx].gold-=1500; db->users[my_id_idx].highest_dmg_weapon = 60; printf("Beli Demon Blade sukses!\n"); }
                        else if(buy!=0) printf("Gold tidak cukup atau pilihan salah!\n");
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
                        printf("Press [ENTER] to return..."); getchar(); getchar();
                    }
                    else if (menu == 4) { send_request(4, my_username, ""); break; }
                }
            } else printf("Login gagal!\n");
        } else break;
    }
    return 0;
}
