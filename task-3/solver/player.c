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
                my_turn = 1;
                printf("\n========================================\n");
                printf("[GILIRAN ANDA]\n\n");
                
                char* own_board = buf + 10;
                char* enemy_board = strchr(own_board, '|');
                if (enemy_board) {
                    *enemy_board = '\0';
                    enemy_board++;
                }

                printf("    Papan Lawan\n");
                printf("  A B C D\n");
                for (int r = 0; r < 4; r++) {
                    printf("%d|", r);
                    for (int c = 0; c < 4; c++) {
                        char cell = enemy_board[r*4 + c];
                        printf("%c|", cell == '.' ? '?' : cell);
                    }
                    printf("\n");
                }
                printf("\n");

                printf("    Papan Anda\n");
                printf("  A B C D\n");
                for (int r = 0; r < 4; r++) {
                    printf("%d|", r);
                    for (int c = 0; c < 4; c++) {
                        char cell = own_board[r*4 + c];
                        printf("%c|", cell == '.' ? ' ' : cell);
                    }
                    printf("\n");
                }
                printf("\n");

                printf("Target (cth: 0A): ");
                fflush(stdout);
            }
            else if (strncmp(buf, "FIRE_RES", 8) == 0) {
                printf("\n[HASIL TEMBAKAN]\n");
                printf("  %s\n", buf + 9);
                printf("========================================\n");
                my_turn = 0;
            }
            else if (strncmp(buf, "OPP_FIRE", 8) == 0) {
                printf("\n[INFO] Lawan menembak %s\n", buf + 9);
            }
            else if (strncmp(buf, "YOU_WIN", 7) == 0) {
                printf("\n========================================\n");
                printf("[HASIL] Semua kapal musuh telah tenggelam!\n");
                printf("[HASIL] ANDA MENANG!\n");
                printf("========================================\n");
                game_over = 1;
            }
            else if (strncmp(buf, "YOU_LOSE", 8) == 0) {
                printf("\n========================================\n");
                printf("[HASIL] Semua kapal Anda telah tenggelam.\n");
                printf("[HASIL] ANDA KALAH.\n");
                printf("========================================\n");
                game_over = 1;
            }

            pthread_mutex_unlock(&console_mutex);
        }
    }
    return NULL;
}

int main(void) {
    printf("[PEMAIN] Menghubungkan ke server...\n");

    pid_t pid = getpid();
    char temp_q[128];
    snprintf(temp_q, sizeof(temp_q), "/mq_bs_temp_%d", pid);
    
    struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10,
                            .mq_msgsize = MSG_SIZE, .mq_curmsgs = 0 };
    mqd_t mq_temp = mq_open(temp_q, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq_temp == (mqd_t)-1) {
        perror("mq_open (temp)"); return 1;
    }

    mqd_t mq_join = mq_open(MQ_BS_JOIN, O_WRONLY);
    if (mq_join == (mqd_t)-1) {
        perror("mq_open (join)");
        printf("Apakah server sudah berjalan?\n");
        mq_close(mq_temp); mq_unlink(temp_q);
        return 1;
    }

    char join_msg[MSG_SIZE];
    snprintf(join_msg, sizeof(join_msg), "CONNECT %s", temp_q);
    mq_send(mq_join, join_msg, strlen(join_msg) + 1, 0);
    mq_close(mq_join);

    char buf[MSG_SIZE];
    memset(buf, 0, MSG_SIZE);
    mq_receive(mq_temp, buf, MSG_SIZE, NULL);
    
    player_id = atoi(buf);
    mq_close(mq_temp);
    mq_unlink(temp_q);

    if (player_id != 1 && player_id != 2) {
        printf("Gagal mendapatkan ID pemain dari server.\n");
        return 1;
    }

    printf("[PEMAIN %d] Berhasil terhubung!\n", player_id);

    const char* mq_send_name = (player_id == 1) ? MQ_P1_TO_SRV : MQ_P2_TO_SRV;
    const char* mq_recv_name = (player_id == 1) ? MQ_SRV_TO_P1 : MQ_SRV_TO_P2;

    mq_out = mq_open(mq_send_name, O_WRONLY);
    mq_in = mq_open(mq_recv_name, O_RDONLY);

    if (mq_out == (mqd_t)-1 || mq_in == (mqd_t)-1) {
        perror("mq_open (game queues)");
        return 1;
    }

    if (player_id == 1) {
        printf("[SERVER] Menunggu Pemain 2 untuk bergabung...\n");
    }

    mq_receive(mq_in, buf, MSG_SIZE, NULL);
    if (strncmp(buf, "GAME_START", 10) == 0) {
        printf("[SERVER] Permainan dimulai (4x4 Sederhana)!\n\n");
    }

    /* Fleet Selection / Placement */
    printf("Anda akan menempatkan 2 kapal (masing-masing 1 petak).\n\n");
    
    char input[100];
    for (int i = 1; i <= 2; i++) {
        while (1) {
            printf("Tempatkan Kapal %d (cth: 0A):\n> ", i);
            fgets(input, sizeof(input), stdin);
            remove_newline(input);
            
            char place_cmd[MSG_SIZE];
            snprintf(place_cmd, sizeof(place_cmd), "PLACE %s", input);
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

    printf("\n[INFO] Menunggu lawan menyelesaikan penempatan...\n");
    mq_receive(mq_in, buf, MSG_SIZE, NULL);
    if (strncmp(buf, "BOARD_INIT", 10) == 0) {
        mq_receive(mq_in, buf, MSG_SIZE, NULL); /* Wait for SETUP_DONE */
    }
    if (strncmp(buf, "SETUP_DONE", 10) == 0) {
        printf("[INFO] Semua pemain siap.\n");
    }

    pthread_t tid;
    pthread_create(&tid, NULL, listener_thread, NULL);

    while (!game_over) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            remove_newline(input);
            pthread_mutex_lock(&console_mutex);
            int is_turn = my_turn;
            pthread_mutex_unlock(&console_mutex);

            if (is_turn && strlen(input) > 0) {
                pthread_mutex_lock(&console_mutex);
                /* Ensure it's roughly 2 chars (e.g. 0A) */
                if (strlen(input) < 2) {
                     printf("Format tidak valid!\nTarget (cth: 0A): ");
                     fflush(stdout);
                     pthread_mutex_unlock(&console_mutex);
                     continue;
                }
                char fire_cmd[MSG_SIZE];
                snprintf(fire_cmd, sizeof(fire_cmd), "FIRE %s", input);
                mq_send(mq_out, fire_cmd, strlen(fire_cmd)+1, 0);
                my_turn = 0;
                pthread_mutex_unlock(&console_mutex);
            }
        }
    }

    pthread_join(tid, NULL);
    mq_close(mq_in);
    mq_close(mq_out);
    return 0;
}
