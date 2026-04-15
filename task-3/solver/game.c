#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MSGSZ   256

#define projGame 'G'

#define typeJoinReq 1
#define typeP1Req 11
#define typeP2Req 12
#define typeP1Res 21
#define typeP2Res 22

typedef struct {
    char own[4][4];
    char shot[4][4];
    int  ships;
} Player;

static Player  players[2];
static int     queueId = -1;
static char    oppMsg[2][MSGSZ];
static int     hasOpp[2];

typedef struct {
    long mtype;
    char text[MSGSZ];
} MsgPacket;

static key_t getKey(int projId) {
    return ftok(".", projId);
}

static int recreateQueue(key_t key) {
    int qid = msgget(key, 0666);
    if (qid >= 0) msgctl(qid, IPC_RMID, NULL);
    return msgget(key, IPC_CREAT | 0666);
}

static long reqType(int p) {
    return p == 0 ? typeP1Req : typeP2Req;
}

static long resType(int p) {
    return p == 0 ? typeP1Res : typeP2Res;
}

static void sendTo(int p, const char *msg) {
    MsgPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.mtype = resType(p);
    strncpy(pkt.text, msg, MSGSZ - 1);
    msgsnd(queueId, &pkt, sizeof(pkt.text), 0);
}

static void recvFrom(int p, char *buf) {
    MsgPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    msgrcv(queueId, &pkt, sizeof(pkt.text), reqType(p), 0);
    memset(buf, 0, MSGSZ);
    strncpy(buf, pkt.text, MSGSZ - 1);
}

static int colIndex(char c) {
    if (c >= 'a' && c <= 'd') return c - 'a';
    if (c >= 'A' && c <= 'D') return c - 'A';
    return -1;
}

static void setup(int p) {
    char buf[MSGSZ];
    printf("[SERVER] Pemain %d sedang menempatkan armada...\n", p + 1);
    for (int i = 1; i <= 2; i++) {
        while (1) {
            recvFrom(p, buf);
            if (strncmp(buf, "PLACE ", 6) != 0) {
                sendTo(p, "ERR Format tidak valid.");
                continue;
            }
            int row = buf[6] - '0';
            int col = colIndex(buf[7]);
            if (row < 0 || row > 3 || col < 0) {
                sendTo(p, "ERR Koordinat di luar batas!");
                continue;
            }
            if (players[p].own[row][col] != '.') {
                sendTo(p, "ERR Petak sudah ditempati!");
                continue;
            }
            players[p].own[row][col] = 'S';
            players[p].ships++;
            char ok[MSGSZ];
            snprintf(ok, MSGSZ, "OK Kapal %d ditempatkan.", i);
            sendTo(p, ok);
            break;
        }
    }
}

static void buildTurn(int p, char *out) {
    int n = sprintf(out, "TURN ");
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[n++] = players[p].own[r][c];
    out[n++] = '|';
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[n++] = players[p].shot[r][c];
    out[n] = '\0';
}

static void fireRound(int shooter) {
    int target = 1 - shooter;
    char buf[MSGSZ];
    while (1) {
        recvFrom(shooter, buf);
        if (strncmp(buf, "FIRE ", 5) != 0) {
            sendTo(shooter, "ERR Format tidak valid.");
            continue;
        }
        int row = buf[5] - '0';
        int col = colIndex(buf[6]);
        if (row < 0 || row > 3 || col < 0) {
            sendTo(shooter, "ERR Koordinat di luar batas!");
            continue;
        }

        char coord[3] = {buf[5], buf[6], '\0'};
        int hit = 0;

        if (players[shooter].shot[row][col] != '.') {
            hit = 0;
        } else if (players[target].own[row][col] == 'S') {
            hit = 1;
            players[target].own[row][col]    = 'X';
            players[shooter].shot[row][col]  = 'X';
            players[target].ships--;
        } else {
            if (players[target].own[row][col] == '.') players[target].own[row][col] = ' ';
            players[shooter].shot[row][col] = ' ';
        }

        printf("[GILIRAN] Pemain %d menembak %s: %s\n", shooter + 1, coord, hit ? "KENA" : "MELESET");
        if (hit && players[target].ships == 0)
            printf("[TENGGELAM] Pemain %d menenggelamkan Kapal Pemain %d!\n", shooter + 1, target + 1);

        char res[MSGSZ];
        snprintf(res, MSGSZ, "RES %s %s, SISA %d KAPAL LAGI",
                 coord, hit ? "KENA KAPAL" : "MELESET", players[target].ships);
        sendTo(shooter, res);

        snprintf(oppMsg[target], MSGSZ, "OPP %s: %s, SISA %d KAPAL LAGI",
                 coord, hit ? "KENA KAPAL" : "MELESET", players[target].ships);
        hasOpp[target] = 1;
        return;
    }
}

static void runGame(void) {
    srand((unsigned)time(NULL));
    int current = rand() % 2;
    printf("[SERVER] Dipilih secara acak: Pemain %d bermain duluan.\n", current + 1);

    char turnBuf[MSGSZ * 2], waitBuf[MSGSZ];
    while (1) {
        int other = 1 - current;

        if (hasOpp[current]) {
            sendTo(current, oppMsg[current]);
            hasOpp[current] = 0;
            usleep(50000);
        }

        buildTurn(current, turnBuf);
        sendTo(current, turnBuf);

        snprintf(waitBuf, MSGSZ, "WAIT Pemain %d sedang bermain...", current + 1);
        sendTo(other, waitBuf);

        fireRound(current);

        if (players[other].ships == 0) {
            sendTo(current, "WIN");
            sendTo(other,   "LOSE");
            printf("[SERVER] Pemain %d menang!\n", current + 1);
            break;
        }
        current = other;
    }
}

static void cleanup(void) {
    if (queueId >= 0) msgctl(queueId, IPC_RMID, NULL);

    printf("[SERVER] Pembersihan selesai. Sampai jumpa.\n");
}

int main(void) {
    key_t key = getKey(projGame);

    if (key == (key_t)-1) {
        perror("ftok");
        return 1;
    }

    printf("[SERVER] Game Master Battleship dimulai (4x4 Sederhana).\n");

    queueId = recreateQueue(key);

    if (queueId < 0) {
        perror("msgget"); cleanup(); return 1;
    }

    memset(players, 0, sizeof(players));
    for (int p = 0; p < 2; p++) {
        memset(players[p].own,  '.', sizeof(players[p].own));
        memset(players[p].shot, '.', sizeof(players[p].shot));
    }

    for (int i = 0; i < 2; i++) {
        MsgPacket pkt;
        char buf[MSGSZ];
        printf("[SERVER] Menunggu Pemain %d...\n", i + 1);
        memset(&pkt, 0, sizeof(pkt));
        msgrcv(queueId, &pkt, sizeof(pkt.text), typeJoinReq, 0);
        strncpy(buf, pkt.text, MSGSZ - 1);

        if (strncmp(buf, "JOIN ", 5) == 0) {
            long replyType = 0;
            if (sscanf(buf + 5, "%ld", &replyType) == 1 && replyType > 0) {
                MsgPacket reply;
                memset(&reply, 0, sizeof(reply));
                reply.mtype = replyType;
                snprintf(reply.text, MSGSZ, "%d", i + 1);
                msgsnd(queueId, &reply, sizeof(reply.text), 0);
            }
            printf("[SERVER] Pemain %d terhubung.\n", i + 1);
        } else {
            i--;
        }
    }

    printf("[SERVER] Kedua pemain terhubung. Permainan dimulai!\n");
    sendTo(0, "START");
    sendTo(1, "START");

    setup(0);
    sendTo(0, "WAIT_OPP");
    setup(1);

    printf("[SERVER] Penempatan armada selesai.\n");
    sendTo(0, "READY");
    sendTo(1, "READY");

    runGame();
    cleanup();
    return 0;
}
