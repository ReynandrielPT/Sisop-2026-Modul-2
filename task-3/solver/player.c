#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MSGSZ   256

#define PROJ_GAME 'G'

#define TYPE_JOIN_REQ 1
#define TYPE_P1_REQ 11
#define TYPE_P2_REQ 12
#define TYPE_P1_RES 21
#define TYPE_P2_RES 22

typedef struct {
    long mtype;
    char text[MSGSZ];
} MsgPacket;

static int     queueId = -1;
static long    reqType = TYPE_P1_REQ;
static long    resType = TYPE_P1_RES;
static int     player_id;
static pthread_mutex_t mutex    = PTHREAD_MUTEX_INITIALIZER;
static volatile int    game_over = 0;
static volatile int    my_turn   = 0;

static key_t get_key(int proj_id) {
    return ftok(".", proj_id);
}

static int recv_msg(int qid, long type, char *out, size_t outsz) {
    MsgPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    if (msgrcv(qid, &pkt, sizeof(pkt.text), type, 0) < 0) return -1;
    memset(out, 0, outsz);
    strncpy(out, pkt.text, outsz - 1);
    return 0;
}

static int send_msg(int qid, long type, const char *text) {
    MsgPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.mtype = type;
    strncpy(pkt.text, text, MSGSZ - 1);
    return msgsnd(qid, &pkt, sizeof(pkt.text), 0);
}

static void strip(char *s) {
    int n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

static void set_stdin_nonblocking(void) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
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
        if (recv_msg(queueId, resType, buf, sizeof(buf)) < 0) {
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

    key_t k_game = get_key(PROJ_GAME);

    if (k_game == (key_t)-1) {
        perror("ftok");
        return 1;
    }

    queueId = msgget(k_game, 0666);
    if (queueId < 0) {
        perror("msgget join");
        printf("Apakah server sudah berjalan?\n");
        return 1;
    }

    char join_msg[MSGSZ];
    long my_type = (long)getpid();
    snprintf(join_msg, MSGSZ, "JOIN %ld", my_type);
    send_msg(queueId, TYPE_JOIN_REQ, join_msg);

    char buf[MSGSZ * 2];
    if (recv_msg(queueId, my_type, buf, sizeof(buf)) < 0) {
        perror("msgrcv join");
        return 1;
    }
    player_id = atoi(buf);

    if (player_id < 1 || player_id > 2) {
        printf("Gagal mendapat ID pemain.\n");
        return 1;
    }

    printf("[PEMAIN %d] Berhasil terhubung!\n", player_id);

    reqType = player_id == 1 ? TYPE_P1_REQ : TYPE_P2_REQ;
    resType = player_id == 1 ? TYPE_P1_RES : TYPE_P2_RES;

    if (player_id == 1)
        printf("[SERVER] Menunggu Pemain 2 untuk bergabung...\n");

    recv_msg(queueId, resType, buf, sizeof(buf));
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
            send_msg(queueId, reqType, cmd);

            recv_msg(queueId, resType, buf, sizeof(buf));

            if (strncmp(buf, "ERR ", 4) == 0) {
                printf("%s\n", buf + 4);
            } else if (strncmp(buf, "OK ", 3) == 0) {
                printf("%s\n", buf + 3);
                break;
            }
        }
    }

    while (1) {
        recv_msg(queueId, resType, buf, sizeof(buf));
        if (strcmp(buf, "WAIT_OPP") == 0) {
            printf("\n[INFO] Menunggu lawan menyelesaikan penempatan...\n");
            continue;
        }
        if (strcmp(buf, "READY") == 0) {
            printf("[INFO] Semua pemain siap.\n");
            break;
        }
    }

    pthread_t tid;
    pthread_create(&tid, NULL, listener, NULL);

    set_stdin_nonblocking();

    while (!game_over) {
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && !game_over) {
                clearerr(stdin);
            }
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
        send_msg(queueId, reqType, cmd);
    }

    pthread_join(tid, NULL);
    return 0;
}
