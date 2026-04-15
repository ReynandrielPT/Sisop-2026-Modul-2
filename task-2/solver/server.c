#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

#define MSGSZ 256

#define T_REG  0
#define T_DATA 1
#define T_EXIT 2

typedef struct {
    int  type;
    int  id;
    int  pid;
    char loc;
    char st;
} SensorMsg;

typedef struct {
    int  id;
    int  pid;
    char status[32];
    int  bye;
} ServerMsg;

typedef struct {
    long mtype;
    SensorMsg body;
} DataPacket;

typedef struct {
    long mtype;
    ServerMsg body;
} StatusPacket;

static key_t key_data(void) {
    return ftok(".", 'D');
}

static key_t key_status(void) {
    return ftok(".", 'S');
}

static int recreate_queue(key_t key) {
    int qid = msgget(key, 0666);
    if (qid >= 0) msgctl(qid, IPC_RMID, NULL);
    return msgget(key, IPC_CREAT | 0666);
}

int main(void) {
    key_t k_data = key_data();
    key_t k_status = key_status();

    if (k_data == (key_t)-1 || k_status == (key_t)-1) {
        perror("ftok");
        return 1;
    }

    int q_in = recreate_queue(k_data);
    int q_out = recreate_queue(k_status);

    if (q_in < 0 || q_out < 0) {
        perror("msgget");
        return 1;
    }

    printf("[SERVER] Waiting for sensors...\n");

    int connected = 0;
    while (connected < 2) {
        DataPacket pkt;
        memset(&pkt, 0, sizeof(pkt));
        msgrcv(q_in, &pkt, sizeof(pkt.body), 0, 0);

        if (pkt.body.type == T_REG) {
            connected++;
            printf("[SERVER] Sensor %d connected\n", connected);

            StatusPacket reply;
            memset(&reply, 0, sizeof(reply));
            reply.mtype = pkt.body.pid;
            reply.body.id  = connected;
            reply.body.pid = pkt.body.pid;
            reply.body.bye = 0;
            msgsnd(q_out, &reply, sizeof(reply.body), 0);
        }
    }

    printf("[SERVER] System ready!\n\n");

    char traffic[4];

    while (1) {
        int cnt    = 0;
        int exited = 0;
        memset(traffic, '?', sizeof(traffic));

        while (cnt < 4) {
            DataPacket pkt;
            memset(&pkt, 0, sizeof(pkt));
            msgrcv(q_in, &pkt, sizeof(pkt.body), 0, 0);
            SensorMsg msg = pkt.body;

            if (msg.type == T_EXIT) {
                exited = 1;
                break;
            }

            if (msg.type == T_DATA) {
                int idx = msg.loc - 'A';
                if (idx >= 0 && idx < 4)
                    traffic[idx] = msg.st;
                cnt++;
            }
        }

        if (exited) {
            printf("[SERVER] Exit signal received\n");
            printf("[SERVER] Shutting down system...\n");

            StatusPacket bye_msg;
            memset(&bye_msg, 0, sizeof(bye_msg));
            bye_msg.body.bye = 1;
            bye_msg.mtype = 1;
            msgsnd(q_out, &bye_msg, sizeof(bye_msg.body), 0);
            bye_msg.mtype = 2;
            msgsnd(q_out, &bye_msg, sizeof(bye_msg.body), 0);
            break;
        }

        printf("[SERVER] Received data:\n");
        printf("  A: %c\n", traffic[0]);
        printf("  B: %c\n", traffic[1]);
        printf("  C: %c\n", traffic[2]);
        printf("  D: %c\n", traffic[3]);

        int high = 0, low = 0;
        for (int i = 0; i < 4; i++) {
            if      (traffic[i] == 'H') high++;
            else if (traffic[i] == 'L') low++;
        }

        printf("\n[SERVER] Summary:\n");
        printf("  High Traffic: %d\n", high);
        printf("  Low Traffic : %d\n", low);

        const char *status;
        if      (high >= 3) status = "MACET TOTAL";
        else if (high == 2) status = "PADAT";
        else                status = "LANCAR";

        printf("\n[SERVER] City Status: %s\n\n", status);

        StatusPacket reply;
        memset(&reply, 0, sizeof(reply));
        strncpy(reply.body.status, status, sizeof(reply.body.status) - 1);
        reply.body.bye = 0;

        reply.mtype = 1;
        msgsnd(q_out, &reply, sizeof(reply.body), 0);
        reply.mtype = 2;
        msgsnd(q_out, &reply, sizeof(reply.body), 0);
    }

    printf("[SERVER] Cleaning up message queue...\n");
    msgctl(q_in, IPC_RMID, NULL);
    msgctl(q_out, IPC_RMID, NULL);
    printf("[SERVER] Done.\n");

    return 0;
}
