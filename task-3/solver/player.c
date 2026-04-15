#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#define MQ_JOIN   "/bs_join"
#define MQ_P1_IN  "/bs_p1_in"
#define MQ_P2_IN  "/bs_p2_in"
#define MQ_P1_OUT "/bs_p1_out"
#define MQ_P2_OUT "/bs_p2_out"
#define MSGSZ   256

static mqd_t   mq_out, mq_in;
static int     player_id;
static pthread_mutex_t mutex    = PTHREAD_MUTEX_INITIALIZER;
static volatile int    game_over = 0;
static volatile int    my_turn   = 0;

static void strip(char *s) {
    int n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

static void show_board(const char *board, int is_enemy) {
    printf("  A B C D\n");
    for (int r = 0; r < 4; r++) {
        printf("%d|", r);
        for (int c = 0; c < 4; c++) {
            char ch = board[r * 4 + c];
            printf("%c|", ch == '.' ? (is_enemy ? '?' : ' ') : ch);
        }
        printf("\n");
    }
}

static void *listener(void *arg) {
    (void)arg;
    char buf[MSGSZ * 2];

    while (!game_over) {
        memset(buf, 0, sizeof(buf));
        if (mq_receive(mq_in, buf, sizeof(buf), NULL) < 0) {
            if (game_over) break;
            continue;
        }

        pthread_mutex_lock(&mutex);

        if (strncmp(buf, "WAIT", 4) == 0) {
            printf("\n[INFO] %s\n", buf + 5);
            fflush(stdout);
            my_turn = 0;
        }
        else if (strncmp(buf, "TURN ", 5) == 0) {
            my_turn = 1;
            char own[17] = {0}, shot[17] = {0};
            char *sep = strchr(buf + 5, '|');
            if (sep) {
                int n = sep - (buf + 5); if (n > 16) n = 16;
                memcpy(own, buf + 5, n);
                memcpy(shot, sep + 1, 16);
            }
            printf("\n========================================\n");
            printf("[GILIRAN ANDA]\n\n");
            printf("    Papan Lawan\n");
            show_board(shot, 1);
            printf("\n    Papan Anda\n");
            show_board(own, 0);
            printf("\nTarget: ");
            fflush(stdout);
        }
        else if (strncmp(buf, "RES ", 4) == 0) {
            printf("\n[HASIL TEMBAKAN]\n%s\n========================================\n", buf + 4);
            fflush(stdout);
            my_turn = 0;
        }
        else if (strncmp(buf, "ERR ", 4) == 0) {
            printf("\n%s\nTarget: ", buf + 4);
            fflush(stdout);
            my_turn = 1;
        }
        else if (strncmp(buf, "OPP ", 4) == 0) {
            printf("\n[INFO] Lawan menembak %s\n", buf + 4);
            fflush(stdout);
        }
        else if (strncmp(buf, "WAIT_OPP", 8) == 0) {
            printf("\n[INFO] Menunggu lawan menyelesaikan penempatan...\n");
            fflush(stdout);
        }
        else if (strcmp(buf, "READY") == 0) {
            printf("[INFO] Semua pemain siap.\n");
            fflush(stdout);
        }
        else if (strcmp(buf, "WIN") == 0) {
            printf("\n========================================\n");
            printf("[HASIL] Semua kapal musuh telah tenggelam!\n");
            printf("[HASIL] ANDA MENANG!\n");
            printf("========================================\n");
            fflush(stdout);
            game_over = 1;
            pthread_mutex_unlock(&mutex);
            break;
        }
        else if (strcmp(buf, "LOSE") == 0) {
            printf("\n========================================\n");
            printf("[HASIL] Semua kapal Anda telah tenggelam.\n");
            printf("[HASIL] ANDA KALAH.\n");
            printf("========================================\n");
            fflush(stdout);
            game_over = 1;
            pthread_mutex_unlock(&mutex);
            break;
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(void) {
    printf("[PEMAIN] Menghubungkan ke server...\n");

    pid_t pid = getpid();
    char  tmp_q[128];
    snprintf(tmp_q, sizeof(tmp_q), "/bs_tmp_%d", (int)pid);

    struct mq_attr attr = {0, 10, MSGSZ, 0};
    mqd_t mq_tmp = mq_open(tmp_q, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq_tmp == (mqd_t)-1) { perror("mq_open tmp"); return 1; }

    mqd_t mq_join = mq_open(MQ_JOIN, O_WRONLY);
    if (mq_join == (mqd_t)-1) {
        perror("mq_open join");
        printf("Apakah server sudah berjalan?\n");
        mq_close(mq_tmp); mq_unlink(tmp_q);
        return 1;
    }

    char join_msg[MSGSZ];
    snprintf(join_msg, MSGSZ, "JOIN %s", tmp_q);
    mq_send(mq_join, join_msg, strlen(join_msg) + 1, 0);
    mq_close(mq_join);

    char buf[MSGSZ * 2];
    memset(buf, 0, sizeof(buf));
    mq_receive(mq_tmp, buf, sizeof(buf), NULL);
    player_id = atoi(buf);
    mq_close(mq_tmp);
    mq_unlink(tmp_q);

    if (player_id < 1 || player_id > 2) {
        printf("Gagal mendapat ID pemain.\n");
        return 1;
    }

    printf("[PEMAIN %d] Berhasil terhubung!\n", player_id);

    mq_out = mq_open(player_id == 1 ? MQ_P1_IN  : MQ_P2_IN,  O_WRONLY);
    mq_in  = mq_open(player_id == 1 ? MQ_P1_OUT : MQ_P2_OUT, O_RDONLY);
    if (mq_out == (mqd_t)-1 || mq_in == (mqd_t)-1) {
        perror("mq_open game queues");
        return 1;
    }

    if (player_id == 1)
        printf("[SERVER] Menunggu Pemain 2 untuk bergabung...\n");

    memset(buf, 0, sizeof(buf));
    mq_receive(mq_in, buf, sizeof(buf), NULL);
    if (strcmp(buf, "START") == 0)
        printf("[SERVER] Permainan dimulai (4x4 Sederhana)!\n\n");

    printf("    Papan Lawan\n");
    printf("  A B C D\n");
    for (int r = 0; r < 4; r++) {
        printf("%d|", r);
        for (int c = 0; c < 4; c++) printf("?|");
        printf("\n");
    }
    printf("\n    Papan Anda\n");
    printf("  A B C D\n");
    for (int r = 0; r < 4; r++) {
        printf("%d|", r);
        for (int c = 0; c < 4; c++) printf(" |");
        printf("\n");
    }
    printf("\n");

    printf("Anda akan menempatkan 2 kapal (masing-masing 1 petak).\n\n");

    char input[128];
    for (int i = 1; i <= 2; i++) {
        while (1) {
            printf("Tempatkan Kapal %d:\n> ", i);
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin) == NULL) continue;
            strip(input);

            char cmd[MSGSZ];
            snprintf(cmd, MSGSZ, "PLACE %s", input);
            mq_send(mq_out, cmd, strlen(cmd) + 1, 0);

            memset(buf, 0, sizeof(buf));
            mq_receive(mq_in, buf, sizeof(buf), NULL);

            if (strncmp(buf, "ERR ", 4) == 0) {
                printf("%s\n", buf + 4);
            } else if (strncmp(buf, "OK ", 3) == 0) {
                printf("%s\n", buf + 3);
                break;
            }
        }
    }

    memset(buf, 0, sizeof(buf));
    mq_receive(mq_in, buf, sizeof(buf), NULL);
    if (strcmp(buf, "WAIT_OPP") == 0)
        printf("\n[INFO] Menunggu lawan menyelesaikan penempatan...\n");

    memset(buf, 0, sizeof(buf));
    mq_receive(mq_in, buf, sizeof(buf), NULL);
    if (strcmp(buf, "READY") == 0)
        printf("[INFO] Semua pemain siap.\n");

    pthread_t tid;
    pthread_create(&tid, NULL, listener, NULL);

    while (!game_over) {
        if (fgets(input, sizeof(input), stdin) == NULL) {
            usleep(100000);
            continue;
        }
        strip(input);

        pthread_mutex_lock(&mutex);
        int turn = my_turn;
        pthread_mutex_unlock(&mutex);

        if (!turn) continue;

        if (strlen(input) < 2) {
            pthread_mutex_lock(&mutex);
            printf("Format tidak valid! Contoh: 2C\nTarget: ");
            fflush(stdout);
            pthread_mutex_unlock(&mutex);
            continue;
        }

        pthread_mutex_lock(&mutex);
        my_turn = 0;
        pthread_mutex_unlock(&mutex);

        char cmd[MSGSZ];
        snprintf(cmd, MSGSZ, "FIRE %s", input);
        mq_send(mq_out, cmd, strlen(cmd) + 1, 0);
    }

    pthread_join(tid, NULL);
    mq_close(mq_in);
    mq_close(mq_out);
    return 0;
}
