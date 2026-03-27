#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>

#define MQ_BS_JOIN    "/mq_bs_join"
#define MQ_P1_TO_SRV  "/mq_bs_p1_srv"
#define MQ_P2_TO_SRV  "/mq_bs_p2_srv"
#define MQ_SRV_TO_P1  "/mq_bs_srv_p1"
#define MQ_SRV_TO_P2  "/mq_bs_srv_p2"
#define MSG_SIZE      512

static mqd_t mq_out, mq_in;
static int player_id;
static pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;
static int game_over = 0;
static int my_turn = 0;

void print_with_mutex(const char* format, ...) {
    pthread_mutex_lock(&console_mutex);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&console_mutex);
}

void remove_newline(char* str) {
    size_t len = strlen(str);
    if (len > 0 && str[len-1] == '\n') str[len-1] = '\0';
}

void* listener_thread(void* arg) {
    char buf[MSG_SIZE];
    while (!game_over) {
        memset(buf, 0, MSG_SIZE);
        if (mq_receive(mq_in, buf, MSG_SIZE, NULL) > 0) {
            pthread_mutex_lock(&console_mutex);
            
            if (strncmp(buf, "WAIT", 4) == 0) {
                printf("\n[INFO] %s\n", buf + 5);
                my_turn = 0;
            }
            else if (strncmp(buf, "YOUR_TURN", 9) == 0) {
                /* Format: YOUR_TURN <own_board:25>|<enemy_board:25>| <cooldowns> */
                my_turn = 1;
                printf("\n========================================\n");
                printf("[GILIRAN ANDA / YOUR TURN]\n\n");
                
                char* own_board = buf + 10;
                char* enemy_board = strchr(own_board, '|');
                if (enemy_board) {
                    *enemy_board = '\0';
                    enemy_board++;
                }
                char* cooldowns = enemy_board ? strchr(enemy_board, '|') : NULL;
                if (cooldowns) {
                    *cooldowns = '\0';
                    cooldowns++;
                }

                printf("    Papan Lawan (Enemy Board)\n");
                printf("  A B C D E\n");
                for (int r = 0; r < 5; r++) {
                    printf("%d|", r);
                    for (int c = 0; c < 5; c++) {
                        char cell = enemy_board[r*5 + c];
                        printf("%c|", cell == '.' ? '?' : cell);
                    }
                    printf("\n");
                }
                printf("\n");

                printf("    Papan Anda (Your Board)\n");
                printf("  A B C D E\n");
                for (int r = 0; r < 5; r++) {
                    printf("%d|", r);
                    for (int c = 0; c < 5; c++) {
                        char cell = own_board[r*5 + c];
                        printf("%c|", cell == '.' ? ' ' : cell);
                    }
                    printf("\n");
                }
                printf("\n");

                printf("Status kapal:\n");
                /* Format cooldowns: " S1:0 D1:2 C1:3 ALL_CD" */
                int all_cd = 0;
                if (cooldowns && strstr(cooldowns, "ALL_CD")) all_cd = 1;
                
                if (cooldowns) {
                    char* tok = strtok(cooldowns, " ");
                    while (tok) {
                        if (strcmp(tok, "ALL_CD") == 0) {
                            tok = strtok(NULL, " ");
                            continue;
                        }
                        char type = tok[0];
                        int idx = tok[1] - '0';
                        char* colon = strchr(tok, ':');
                        if (colon) {
                            int cd = atoi(colon + 1);
                            const char* sname = (type == 'S') ? "Submarine" : (type == 'D') ? "Destroyer" : "Cruiser";
                            if (cd == 0) {
                                printf("  %c%d (%s %d) : SIAP / READY\n", type, idx, sname, idx);
                            } else {
                                printf("  %c%d (%s %d) : COOLDOWN (%d turn(s) remaining)\n", type, idx, sname, idx, cd);
                            }
                        }
                        tok = strtok(NULL, " ");
                    }
                }
                printf("\n");

                if (all_cd) {
                    printf("[INFO] All of your ships are on cooldown. Your turn is automatically skipped.\n");
                    printf("========================================\n");
                    mq_send(mq_out, "SKIP", 5, 0);
                    my_turn = 0;
                } else {
                    printf("Pilih kapal / Select a ship (e.g., S1, D1): ");
                    fflush(stdout);
                }
            }
            else if (strncmp(buf, "FIRE_RES", 8) == 0) {
                printf("\n[SHOT RESULT]\n");
                printf("  %s\n", buf + 9);
                printf("========================================\n");
                my_turn = 0;
            }
            else if (strncmp(buf, "OPP_FIRE", 8) == 0) {
                /* OPP_FIRE C 1A, 2B, 3C HITS:1 MISSES:2 SUNK:Submarine */
                printf("\n[INFO] Opponent fired using %c at %s\n", buf[9], buf + 11);
            }
            else if (strncmp(buf, "YOU_WIN", 7) == 0) {
                printf("\n========================================\n");
                printf("[RESULT] All enemy ships have been sunk!\n");
                printf("[RESULT] YOU WIN!\n");
                printf("========================================\n");
                game_over = 1;
            }
            else if (strncmp(buf, "YOU_LOSE", 8) == 0) {
                printf("\n========================================\n");
                printf("[RESULT] All your ships have been sunk.\n");
                printf("[RESULT] YOU LOSE.\n");
                printf("========================================\n");
                game_over = 1;
            }

            pthread_mutex_unlock(&console_mutex);
        }
    }
    return NULL;
}

int main(void) {
    printf("[PLAYER] Connecting to server...\n");

    /* Create a temporary queue to receive our assigned ID */
    pid_t pid = getpid();
    char temp_q[128];
    snprintf(temp_q, sizeof(temp_q), "/mq_bs_temp_%d", pid);
    
    struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10,
                            .mq_msgsize = MSG_SIZE, .mq_curmsgs = 0 };
    mqd_t mq_temp = mq_open(temp_q, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq_temp == (mqd_t)-1) {
        perror("mq_open (temp)"); return 1;
    }

    /* Send connection request to server's join queue */
    mqd_t mq_join = mq_open(MQ_BS_JOIN, O_WRONLY);
    if (mq_join == (mqd_t)-1) {
        perror("mq_open (join)");
        printf("Is the server running?\n");
        mq_close(mq_temp); mq_unlink(temp_q);
        return 1;
    }

    char join_msg[MSG_SIZE];
    snprintf(join_msg, sizeof(join_msg), "CONNECT %s", temp_q);
    mq_send(mq_join, join_msg, strlen(join_msg) + 1, 0);
    mq_close(mq_join);

    /* Wait for server to assign an ID ("1" or "2") */
    char buf[MSG_SIZE];
    memset(buf, 0, MSG_SIZE);
    mq_receive(mq_temp, buf, MSG_SIZE, NULL);
    
    player_id = atoi(buf);
    mq_close(mq_temp);
    mq_unlink(temp_q);

    if (player_id != 1 && player_id != 2) {
        printf("Failed to get a valid player ID from server.\n");
        return 1;
    }

    printf("[PLAYER %d] Connected successfully!\n", player_id);

    const char* mq_send_name = (player_id == 1) ? MQ_P1_TO_SRV : MQ_P2_TO_SRV;
    const char* mq_recv_name = (player_id == 1) ? MQ_SRV_TO_P1 : MQ_SRV_TO_P2;

    mq_out = mq_open(mq_send_name, O_WRONLY);
    mq_in = mq_open(mq_recv_name, O_RDONLY);

    if (mq_out == (mqd_t)-1 || mq_in == (mqd_t)-1) {
        perror("mq_open (game queues)");
        return 1;
    }

    if (player_id == 1) {
        printf("[SERVER] Waiting for Player 2 to connect...\n");
    }

    /* Wait for GAME_START */
    mq_receive(mq_in, buf, MSG_SIZE, NULL);
    if (strncmp(buf, "GAME_START", 10) == 0) {
        printf("[SERVER] Game starting!\n\n");
    }

    /* Fleet Selection */
    while (1) {
        printf("Select your fleet (Max 7 points):\n");
        printf("  S (Submarine) - 2 pts | 2 tiles | fires 1 tile  | cooldown: none\n");
        printf("  D (Destroyer) - 2 pts | 2 tiles | fires 2 tiles | cooldown: 2 turns\n");
        printf("  C (Cruiser)   - 3 pts | 3 tiles | fires 3 tiles | cooldown: 3 turns\n\n");
        printf("Enter your selection (example: 2S 1D or 1S 1D 1C): ");
        
        char input[100];
        fgets(input, sizeof(input), stdin);
        remove_newline(input);

        mq_send(mq_out, input, strlen(input)+1, 0);

        mq_receive(mq_in, buf, MSG_SIZE, NULL);
        if (strncmp(buf, "FLEET_ERR", 9) == 0) {
            printf("\n%s\n\n", buf);
        } else if (strncmp(buf, "FLEET_OK", 8) == 0) {
            printf("\nFleet confirmed.\n");
            
            int ns=0, nd=0, nc=0;
            sscanf(buf + 9, "%dS %dD %dC", &ns, &nd, &nc);
            
            char order[15];
            int order_idx[15];
            int oc = 0;
            for(int i=1; i<=ns; i++) { order[oc] = 'S'; order_idx[oc++] = i; }
            for(int i=1; i<=nd; i++) { order[oc] = 'D'; order_idx[oc++] = i; }
            for(int i=1; i<=nc; i++) { order[oc] = 'C'; order_idx[oc++] = i; }

            for (int si = 0; si < oc; si++) {
                while (1) {
                    const char* sname = (order[si] == 'S') ? "Submarine" : (order[si] == 'D') ? "Destroyer" : "Cruiser";
                    if (ns+nd+nc > 1) {
                        printf("\nPlace %s %d (2 edge coordinates, e.g. 0A 0B):\n> ", sname, order_idx[si]);
                    } else {
                        printf("\nPlace %s (2 edge coordinates, e.g. 0A 0B):\n> ", sname);
                    }
                    fgets(input, sizeof(input), stdin);
                    remove_newline(input);
                    
                    char coord1[8]="", coord2[8]="";
                    sscanf(input, "%s %s", coord1, coord2);

                    char place_cmd[MSG_SIZE];
                    snprintf(place_cmd, sizeof(place_cmd), "%c %d %s %s", order[si], order_idx[si], coord1, coord2);
                    mq_send(mq_out, place_cmd, strlen(place_cmd)+1, 0);

                    mq_receive(mq_in, buf, MSG_SIZE, NULL);
                    if (strncmp(buf, "PLACE_ERR", 9) == 0) {
                        printf("%s\n", buf + 10);
                    } else if (strncmp(buf, "PLACE_OK", 8) == 0) {
                        printf("%s\n", buf + 9);
                        break;
                    }
                }
            }
            break; /* Setup done */
        }
    }

    printf("\n[INFO] Waiting for opponent to finish placement...\n");
    mq_receive(mq_in, buf, MSG_SIZE, NULL);
    /* Should receive BOARD_INIT ... then SETUP_DONE */
    if (strncmp(buf, "BOARD_INIT", 10) == 0) {
        mq_receive(mq_in, buf, MSG_SIZE, NULL); /* Wait for SETUP_DONE */
    }
    if (strncmp(buf, "SETUP_DONE", 10) == 0) {
        printf("[INFO] All players ready.\n");
    }

    /* Start listener thread for asynchronous game events */
    pthread_t tid;
    pthread_create(&tid, NULL, listener_thread, NULL);

    /* Main input loop */
    char input[100];
    while (!game_over) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            remove_newline(input);
            pthread_mutex_lock(&console_mutex);
            int is_turn = my_turn;
            pthread_mutex_unlock(&console_mutex);

            if (is_turn && strlen(input) > 0) {
                char ship_sel[8] = {0};
                char coords[10][8];
                int num_coords = 0;
                
                char *tok = strtok(input, " ");
                if (tok) {
                    strncpy(ship_sel, tok, 7);
                    tok = strtok(NULL, " ");
                    while (tok && num_coords < 10) {
                        strncpy(coords[num_coords], tok, 7);
                        num_coords++;
                        tok = strtok(NULL, " ");
                    }
                }
                
                char type = toupper(ship_sel[0]);
                int target_count = (type == 'S') ? 1 : (type == 'D') ? 2 : (type == 'C') ? 3 : 0;
                
                if (target_count > 0) {
                    if (num_coords > target_count) {
                        pthread_mutex_lock(&console_mutex);
                        printf("Jumlah shot terlalu banyak!\nPilih kapal / Select a ship (e.g., S1, D1): ");
                        fflush(stdout);
                        pthread_mutex_unlock(&console_mutex);
                        continue;
                    }
                    
                    while (num_coords < target_count) {
                        pthread_mutex_lock(&console_mutex);
                        printf("Target %d (%d shot(s) remaining): ", num_coords + 1, target_count - num_coords);
                        fflush(stdout);
                        pthread_mutex_unlock(&console_mutex);
                        
                        char extra_input[100];
                        if (fgets(extra_input, sizeof(extra_input), stdin) != NULL) {
                            remove_newline(extra_input);
                            char *etok = strtok(extra_input, " ");
                            while (etok && num_coords < 10) {
                                strncpy(coords[num_coords], etok, 7);
                                num_coords++;
                                etok = strtok(NULL, " ");
                            }
                        }
                        
                        if (num_coords > target_count) {
                            pthread_mutex_lock(&console_mutex);
                            printf("Jumlah shot terlalu banyak!\nPilih kapal / Select a ship (e.g., S1, D1): ");
                            fflush(stdout);
                            pthread_mutex_unlock(&console_mutex);
                            break;
                        }
                    }
                    
                    if (num_coords > target_count) continue;
                    
                    char coords_str[MSG_SIZE] = "";
                    for (int i=0; i<num_coords; i++) {
                        if (i > 0) strcat(coords_str, " ");
                        strcat(coords_str, coords[i]);
                    }
                    
                    pthread_mutex_lock(&console_mutex);
                    char fire_cmd[MSG_SIZE];
                    ship_sel[0] = type; 
                    snprintf(fire_cmd, sizeof(fire_cmd), "%s %s", ship_sel, coords_str);
                    mq_send(mq_out, fire_cmd, strlen(fire_cmd)+1, 0);
                    my_turn = 0;
                    pthread_mutex_unlock(&console_mutex);
                } else {
                    pthread_mutex_lock(&console_mutex);
                    printf("Invalid ship type. Select a ship (e.g., S1, D1): ");
                    fflush(stdout);
                    pthread_mutex_unlock(&console_mutex);
                }
            }
        }
    }

    pthread_join(tid, NULL);
    mq_close(mq_in);
    mq_close(mq_out);
    return 0;
}
