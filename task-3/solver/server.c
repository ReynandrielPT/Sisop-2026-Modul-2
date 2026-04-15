#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MQ_JOIN   "/bs_join"
#define MQ_P1_IN  "/bs_p1_in"
#define MQ_P2_IN  "/bs_p2_in"
#define MQ_P1_OUT "/bs_p1_out"
#define MQ_P2_OUT "/bs_p2_out"
#define MSGSZ   256

typedef struct {
    char own[4][4];
    char shot[4][4];
    int  ships;
} Player;

static Player  player[2];
static mqd_t   mq_in[2], mq_out[2];
static char    opp_msg[2][MSGSZ];
static int     has_opp[2];

static void send_to(int p, const char *msg) {
    mq_send(mq_out[p], msg, strlen(msg) + 1, 0);
}

static void recv_from(int p, char *buf) {
    memset(buf, 0, MSGSZ);
    mq_receive(mq_in[p], buf, MSGSZ, NULL);
}

static int col_idx(char c) {
    if (c >= 'a' && c <= 'd') return c - 'a';
    if (c >= 'A' && c <= 'D') return c - 'A';
    return -1;
}

static void setup(int p) {
    char buf[MSGSZ];
    printf("[SERVER] Pemain %d sedang menempatkan armada...\n", p + 1);
    for (int i = 1; i <= 2; i++) {
        while (1) {
            recv_from(p, buf);
            if (strncmp(buf, "PLACE ", 6) != 0) {
                send_to(p, "ERR Format tidak valid.");
                continue;
            }
            int row = buf[6] - '0';
            int col = col_idx(buf[7]);
            if (row < 0 || row > 3 || col < 0) {
                send_to(p, "ERR Koordinat di luar batas!");
                continue;
            }
            if (player[p].own[row][col] != '.') {
                send_to(p, "ERR Petak sudah ditempati!");
                continue;
            }
            player[p].own[row][col] = 'S';
            player[p].ships++;
            char ok[MSGSZ];
            snprintf(ok, MSGSZ, "OK Kapal %d ditempatkan.", i);
            send_to(p, ok);
            break;
        }
    }
}

static void build_turn(int p, char *out) {
    int n = sprintf(out, "TURN ");
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[n++] = player[p].own[r][c];
    out[n++] = '|';
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[n++] = player[p].shot[r][c];
    out[n] = '\0';
}

static void fire_round(int shooter) {
    int target = 1 - shooter;
    char buf[MSGSZ];
    while (1) {
        recv_from(shooter, buf);
        if (strncmp(buf, "FIRE ", 5) != 0) {
            send_to(shooter, "ERR Format tidak valid.");
            continue;
        }
        int row = buf[5] - '0';
        int col = col_idx(buf[6]);
        if (row < 0 || row > 3 || col < 0) {
            send_to(shooter, "ERR Koordinat di luar batas!");
            continue;
        }

        char coord[3] = {buf[5], buf[6], '\0'};
        int hit = 0;

        if (player[shooter].shot[row][col] != '.') {
            hit = 0;
        } else if (player[target].own[row][col] == 'S') {
            hit = 1;
            player[target].own[row][col]    = 'X';
            player[shooter].shot[row][col]  = 'X';
            player[target].ships--;
        } else {
            if (player[target].own[row][col] == '.') player[target].own[row][col] = ' ';
            player[shooter].shot[row][col] = ' ';
        }

        printf("[GILIRAN] Pemain %d menembak %s: %s\n", shooter + 1, coord, hit ? "KENA" : "MELESET");
        if (hit && player[target].ships == 0)
            printf("[TENGGELAM] Pemain %d menenggelamkan Kapal Pemain %d!\n", shooter + 1, target + 1);

        char res[MSGSZ];
        snprintf(res, MSGSZ, "RES %s %s, SISA %d KAPAL LAGI",
                 coord, hit ? "KENA KAPAL" : "MELESET", player[target].ships);
        send_to(shooter, res);

        snprintf(opp_msg[target], MSGSZ, "OPP %s: %s, SISA %d KAPAL LAGI",
                 coord, hit ? "KENA KAPAL" : "MELESET", player[target].ships);
        has_opp[target] = 1;
        return;
    }
}

static void run_game(void) {
    srand((unsigned)time(NULL));
    int current = rand() % 2;
    printf("[SERVER] Dipilih secara acak: Pemain %d bermain duluan.\n", current + 1);

    char turn_buf[MSGSZ * 2], wait_buf[MSGSZ];
    while (1) {
        int other = 1 - current;

        if (has_opp[current]) {
            send_to(current, opp_msg[current]);
            has_opp[current] = 0;
            usleep(50000);
        }

        build_turn(current, turn_buf);
        send_to(current, turn_buf);

        snprintf(wait_buf, MSGSZ, "WAIT Pemain %d sedang bermain...", current + 1);
        send_to(other, wait_buf);

        fire_round(current);

        if (player[other].ships == 0) {
            send_to(current, "WIN");
            send_to(other,   "LOSE");
            printf("[SERVER] Pemain %d menang!\n", current + 1);
            break;
        }
        current = other;
    }
}

static void cleanup(void) {
    mq_close(mq_in[0]);  mq_unlink(MQ_P1_IN);
    mq_close(mq_in[1]);  mq_unlink(MQ_P2_IN);
    mq_close(mq_out[0]); mq_unlink(MQ_P1_OUT);
    mq_close(mq_out[1]); mq_unlink(MQ_P2_OUT);
    printf("[SERVER] Pembersihan selesai. Sampai jumpa.\n");
}

int main(void) {
    struct mq_attr attr = {0, 10, MSGSZ, 0};

    mq_unlink(MQ_JOIN); mq_unlink(MQ_P1_IN); mq_unlink(MQ_P2_IN);
    mq_unlink(MQ_P1_OUT); mq_unlink(MQ_P2_OUT);

    printf("[SERVER] Game Master Battleship dimulai (4x4 Sederhana).\n");

    mq_in[0]  = mq_open(MQ_P1_IN,  O_CREAT | O_RDONLY, 0666, &attr);
    mq_in[1]  = mq_open(MQ_P2_IN,  O_CREAT | O_RDONLY, 0666, &attr);
    mq_out[0] = mq_open(MQ_P1_OUT, O_CREAT | O_WRONLY, 0666, &attr);
    mq_out[1] = mq_open(MQ_P2_OUT, O_CREAT | O_WRONLY, 0666, &attr);
    mqd_t mq_join = mq_open(MQ_JOIN, O_CREAT | O_RDONLY, 0666, &attr);

    if (mq_in[0]==(mqd_t)-1 || mq_in[1]==(mqd_t)-1 ||
        mq_out[0]==(mqd_t)-1|| mq_out[1]==(mqd_t)-1 || mq_join==(mqd_t)-1) {
        perror("mq_open"); cleanup(); return 1;
    }

    memset(player, 0, sizeof(player));
    for (int p = 0; p < 2; p++) {
        memset(player[p].own,  '.', sizeof(player[p].own));
        memset(player[p].shot, '.', sizeof(player[p].shot));
    }

    for (int i = 0; i < 2; i++) {
        char buf[MSGSZ];
        printf("[SERVER] Menunggu Pemain %d...\n", i + 1);
        memset(buf, 0, MSGSZ);
        mq_receive(mq_join, buf, MSGSZ, NULL);

        if (strncmp(buf, "JOIN ", 5) == 0) {
            char tmp_q[128];
            sscanf(buf + 5, "%127s", tmp_q);
            mqd_t mq_tmp = mq_open(tmp_q, O_WRONLY);
            if (mq_tmp != (mqd_t)-1) {
                char id[4]; snprintf(id, 4, "%d", i + 1);
                mq_send(mq_tmp, id, strlen(id) + 1, 0);
                mq_close(mq_tmp);
            }
            printf("[SERVER] Pemain %d terhubung.\n", i + 1);
        } else {
            i--;
        }
    }

    mq_close(mq_join);
    mq_unlink(MQ_JOIN);

    printf("[SERVER] Kedua pemain terhubung. Permainan dimulai!\n");
    send_to(0, "START");
    send_to(1, "START");

    setup(0);
    send_to(0, "WAIT_OPP");
    setup(1);

    printf("[SERVER] Penempatan armada selesai.\n");
    send_to(0, "READY");
    send_to(1, "READY");

    run_game();
    cleanup();
    return 0;
}
