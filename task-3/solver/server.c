#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ─── Queue names ─────────────────────────────────────────────────────────── */
#define MQ_BS_JOIN    "/mq_bs_join"
#define MQ_P1_TO_SRV  "/mq_bs_p1_srv"
#define MQ_P2_TO_SRV  "/mq_bs_p2_srv"
#define MQ_SRV_TO_P1  "/mq_bs_srv_p1"
#define MQ_SRV_TO_P2  "/mq_bs_srv_p2"
#define MSG_SIZE      512

/* ─── Game constants ──────────────────────────────────────────────────────── */
#define ROWS          4
#define COLS          4
#define MAX_SHIPS     2

/* ─── Cell markers ────────────────────────────────────────────────────────── */
#define CELL_EMPTY   '.'
#define CELL_HIT     'X'
#define CELL_MISS    ' '
#define CELL_SHIP    'S'

/* ─── Structures ──────────────────────────────────────────────────────────── */
typedef struct {
    int  r, c;          /* row, col of the tile */
    int  sunk;          /* 1 if hit/sunk */
    int  index;         /* 1 or 2 */
} Ship;

typedef struct {
    char board[ROWS][COLS];
    char hit_board[ROWS][COLS];
    Ship ships[MAX_SHIPS];
    int  num_ships;
    int  ships_remaining;
    int  connected;
} Player;

/* ─── Globals ────────────────────────────────────────────────────────────── */
static Player players[2];
static mqd_t mq_in[2], mq_out[2];
static char pending_opp_msg[2][MSG_SIZE];
static int has_pending_opp_msg[2] = {0, 0};

/* ─── Utilities ──────────────────────────────────────────────────────────── */
static int col_to_idx(char c) {
    if (c >= 'a' && c <= 'd') return c - 'a';
    if (c >= 'A' && c <= 'D') return c - 'A';
    return -1;
}

static void srv_send(int pid, const char *msg) {
    mq_send(mq_out[pid], msg, strlen(msg) + 1, 0);
}

static void srv_recv(int pid, char *buf) {
    memset(buf, 0, MSG_SIZE);
    mq_receive(mq_in[pid], buf, MSG_SIZE, NULL);
}

/* ─── Board helpers ──────────────────────────────────────────────────────── */
static void init_board(Player *p) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            p->board[r][c]     = CELL_EMPTY;
            p->hit_board[r][c] = CELL_EMPTY;
        }
    p->num_ships = 0;
    p->ships_remaining = 0;
}

static int place_ship(Player *p, int idx, int r, int c) {
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return -3; /* out of bounds */
    if (p->board[r][c] != CELL_EMPTY) return -4;             /* occupied */

    Ship *s = &p->ships[p->num_ships];
    s->r = r;
    s->c = c;
    s->sunk = 0;
    s->index = idx;

    p->board[r][c] = CELL_SHIP;
    p->num_ships++;
    p->ships_remaining++;
    return 0;
}

static void setup_player(int pid) {
    Player *p = &players[pid];
    char buf[MSG_SIZE];

    for (int i = 1; i <= MAX_SHIPS; i++) {
        while (1) {
            srv_recv(pid, buf);
            /* Expect "PLACE 0A" */
            char coord_str[8];
            if (sscanf(buf, "PLACE %s", coord_str) != 1) {
                srv_send(pid, "PLACE_ERR Format tidak valid.");
                continue;
            }

            int r = coord_str[0] - '0';
            int c = col_to_idx(coord_str[1]);

            if (r < 0 || r >= ROWS || c < 0 || c >= COLS) {
                srv_send(pid, "PLACE_ERR Koordinat di luar batas!");
                continue;
            }

            int res = place_ship(p, i, r, c);
            if (res == -3) {
                srv_send(pid, "PLACE_ERR Koordinat di luar batas!");
            } else if (res == -4) {
                srv_send(pid, "PLACE_ERR Petak sudah ditempati!");
            } else {
                char ok[MSG_SIZE];
                snprintf(ok, MSG_SIZE, "PLACE_OK Kapal %d ditempatkan.", i);
                srv_send(pid, ok);
                break;
            }
        }
    }
    /* Send board state to player */
    char board_msg[MSG_SIZE * 4];
    int bm = 0;
    bm += snprintf(board_msg + bm, sizeof(board_msg) - bm, "BOARD_INIT ");
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            bm += snprintf(board_msg + bm, sizeof(board_msg) - bm, "%c", p->board[r][c]);
    srv_send(pid, board_msg);
}

/* ─── Fire processing ────────────────────────────────────────────────────── */
static void process_fire(int shooter_pid) {
    int target_pid = 1 - shooter_pid;
    Player *shooter = &players[shooter_pid];
    Player *target  = &players[target_pid];
    char buf[MSG_SIZE];

    while (1) {
        srv_recv(shooter_pid, buf);
        /* Expect "FIRE 0A" */

        char coord_str[8];
        if (sscanf(buf, "FIRE %s", coord_str) != 1) {
            srv_send(shooter_pid, "FIRE_ERR Format tidak valid.");
            continue;
        }

        if (strlen(coord_str) != 2) {
            srv_send(shooter_pid, "FIRE_ERR Format koordinat tidak valid.");
            continue;
        }

        int r = coord_str[0] - '0';
        int c = col_to_idx(coord_str[1]);
        if (r < 0 || r >= ROWS || c < 0 || c >= COLS) {
            srv_send(shooter_pid, "FIRE_ERR Koordinat di luar batas!");
            continue;
        }

        /* Process the shot */
        int hit = 0;
        int sunk = 0;

        if (target->board[r][c] == CELL_SHIP) {
            hit = 1;
            target->hit_board[r][c] = CELL_HIT;
            
            for (int i = 0; i < target->num_ships; i++) {
                if (target->ships[i].r == r && target->ships[i].c == c && !target->ships[i].sunk) {
                    target->ships[i].sunk = 1;
                    target->ships_remaining--;
                    sunk = 1;
                    break;
                }
            }
        } else {
            target->hit_board[r][c] = CELL_MISS;
        }

        /* Server console log */
        printf("[GILIRAN] Pemain %d menembak %s: %s\n", shooter_pid + 1, coord_str, hit ? "KENA" : "MELESET");
        if (sunk) {
            printf("  [TENGGELAM] Pemain %d menenggelamkan Kapal Pemain %d!\n", shooter_pid + 1, target_pid + 1);
        }

        /* Notify shooter */
        char res_msg[MSG_SIZE];
        if (hit) {
            snprintf(res_msg, MSG_SIZE, "FIRE_RES %s KENA KAPAL, SISA %d KAPAL LAGI", coord_str, target->ships_remaining);
            snprintf(pending_opp_msg[target_pid], MSG_SIZE, "OPP_FIRE %s: KENA KAPAL, SISA %d KAPAL LAGI", coord_str, target->ships_remaining);
        } else {
            snprintf(res_msg, MSG_SIZE, "FIRE_RES %s MELESET, SISA %d KAPAL LAGI", coord_str, target->ships_remaining);
            snprintf(pending_opp_msg[target_pid], MSG_SIZE, "OPP_FIRE %s: MELESET, SISA %d KAPAL LAGI", coord_str, target->ships_remaining);
        }
        has_pending_opp_msg[target_pid] = 1;
        srv_send(shooter_pid, res_msg);

        return;
    }
}

/* ─── Turn loop ──────────────────────────────────────────────────────────── */
static void build_turn_msg(int pid, char *out, size_t osz) {
    Player *p = &players[pid];
    int n = 0;
    n += snprintf(out + n, osz - n, "YOUR_TURN ");
    
    /* Append own board */
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char cell = p->board[r][c];
            if (p->hit_board[r][c] == CELL_HIT) cell = CELL_HIT;
            else if (p->hit_board[r][c] == CELL_MISS) cell = CELL_MISS;
            out[n++] = cell;
        }
    }
    out[n++] = '|';
    /* Append enemy hit board */
    Player *opp = &players[1 - pid];
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            out[n++] = opp->hit_board[r][c];
    out[n++] = '|';
    out[n] = '\0';
}

static void run_game(void) {
    srand((unsigned)time(NULL));
    int current = rand() % 2;
    printf("[SERVER] Dipilih secara acak: Pemain %d bermain duluan.\n", current + 1);

    char turn_msg[MSG_SIZE * 8];
    char wait_msg[MSG_SIZE];

    while (1) {
        int other = 1 - current;

        if (has_pending_opp_msg[current]) {
            srv_send(current, pending_opp_msg[current]);
            has_pending_opp_msg[current] = 0;
            usleep(50000); // 50ms delay to keep prints sequential
        }

        build_turn_msg(current, turn_msg, sizeof(turn_msg));
        srv_send(current, turn_msg);

        snprintf(wait_msg, sizeof(wait_msg), "WAIT Pemain %d sedang bermain...", current + 1);
        srv_send(other, wait_msg);

        process_fire(current);

        if (players[other].ships_remaining == 0) {
            srv_send(current, "YOU_WIN");
            srv_send(other,   "YOU_LOSE");
            printf("[SERVER] Pemain %d menang!\n", current + 1);
            break;
        }

        current = other;
    }
}

/* ─── Cleanup ────────────────────────────────────────────────────────────── */
static void cleanup(void) {
    mq_close(mq_in[0]);  mq_unlink(MQ_P1_TO_SRV);
    mq_close(mq_in[1]);  mq_unlink(MQ_P2_TO_SRV);
    mq_close(mq_out[0]); mq_unlink(MQ_SRV_TO_P1);
    mq_close(mq_out[1]); mq_unlink(MQ_SRV_TO_P2);
    printf("[SERVER] Pembersihan selesai. Sampai jumpa.\n");
}

/* ─── Main ───────────────────────────────────────────────────────────────── */
int main(void) {
    struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10,
                            .mq_msgsize = MSG_SIZE, .mq_curmsgs = 0 };

    mq_unlink(MQ_BS_JOIN);
    mq_unlink(MQ_P1_TO_SRV); mq_unlink(MQ_P2_TO_SRV);
    mq_unlink(MQ_SRV_TO_P1); mq_unlink(MQ_SRV_TO_P2);

    printf("[SERVER] Game Master Battleship dimulai (4x4 Sederhana).\n");

    mq_in[0]  = mq_open(MQ_P1_TO_SRV, O_CREAT | O_RDONLY, 0666, &attr);
    mq_in[1]  = mq_open(MQ_P2_TO_SRV, O_CREAT | O_RDONLY, 0666, &attr);
    mq_out[0] = mq_open(MQ_SRV_TO_P1, O_CREAT | O_WRONLY, 0666, &attr);
    mq_out[1] = mq_open(MQ_SRV_TO_P2, O_CREAT | O_WRONLY, 0666, &attr);
    mqd_t mq_join = mq_open(MQ_BS_JOIN, O_CREAT | O_RDONLY, 0666, &attr);

    if (mq_in[0] == (mqd_t)-1 || mq_in[1] == (mqd_t)-1 ||
        mq_out[0] == (mqd_t)-1 || mq_out[1] == (mqd_t)-1 || mq_join == (mqd_t)-1) {
        perror("mq_open"); cleanup(); return 1;
    }

    memset(players, 0, sizeof(players));
    init_board(&players[0]);
    init_board(&players[1]);

    for (int i = 0; i < 2; i++) {
        char buf[MSG_SIZE];
        printf("[SERVER] Menunggu Pemain %d...\n", i + 1);
        memset(buf, 0, MSG_SIZE);
        mq_receive(mq_join, buf, MSG_SIZE, NULL);
        
        if (strncmp(buf, "CONNECT ", 8) == 0) {
            char temp_q[128];
            sscanf(buf + 8, "%s", temp_q);
            
            mqd_t mq_temp = mq_open(temp_q, O_WRONLY);
            if (mq_temp != (mqd_t)-1) {
                char reply[32];
                snprintf(reply, sizeof(reply), "%d", i + 1);
                mq_send(mq_temp, reply, strlen(reply) + 1, 0);
                mq_close(mq_temp);
            }
            
            players[i].connected = 1;
            printf("[SERVER] Pemain %d terhubung.\n", i + 1);
        } else {
            i--; 
        }
    }
    
    mq_close(mq_join);
    mq_unlink(MQ_BS_JOIN);
    
    printf("[SERVER] Kedua pemain terhubung. Permainan dimulai!\n");
    srv_send(0, "GAME_START");
    srv_send(1, "GAME_START");

    printf("[SERVER] Pemain 1 sedang menempatkan armada...\n");
    setup_player(0);
    printf("[SERVER] Pemain 2 sedang menempatkan armada...\n");
    setup_player(1);
    printf("[SERVER] Penempatan armada selesai.\n");
    srv_send(0, "SETUP_DONE");
    srv_send(1, "SETUP_DONE");

    run_game();
    cleanup();
    return 0;
}
