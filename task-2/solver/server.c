#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MQ_DATA   "/stc_data"
#define MQ_STATUS "/stc_status"
#define MSGSZ    256

#define T_REG  0
#define T_DATA 1
#define T_EXIT 2

typedef struct {
    int  type;
    int  id;
    char loc;
    char st;
} SensorMsg;

typedef struct {
    int  id;
    char status[32];
    int  bye;
} ServerMsg;

int main(void) {
    struct mq_attr attr = {0, 10, MSGSZ, 0};

    mq_unlink(MQ_DATA);
    mq_unlink(MQ_STATUS);

    mqd_t mq_in  = mq_open(MQ_DATA,   O_CREAT | O_RDONLY, 0666, &attr);
    mqd_t mq_out = mq_open(MQ_STATUS, O_CREAT | O_WRONLY, 0666, &attr);

    if (mq_in == (mqd_t)-1 || mq_out == (mqd_t)-1) {
        perror("mq_open");
        return 1;
    }

    printf("[SERVER] Waiting for sensors...\n");

    int connected = 0;
    while (connected < 2) {
        SensorMsg msg;
        memset(&msg, 0, sizeof(msg));
        mq_receive(mq_in, (char *)&msg, MSGSZ, NULL);

        if (msg.type == T_REG) {
            connected++;
            printf("[SERVER] Sensor %d connected\n", connected);

            ServerMsg reply;
            memset(&reply, 0, sizeof(reply));
            reply.id  = connected;
            reply.bye = 0;
            mq_send(mq_out, (char *)&reply, sizeof(reply), 0);
        }
    }

    printf("[SERVER] System ready!\n\n");

    char traffic[4];

    while (1) {
        int cnt    = 0;
        int exited = 0;
        memset(traffic, '?', sizeof(traffic));

        while (cnt < 4) {
            SensorMsg msg;
            memset(&msg, 0, sizeof(msg));
            mq_receive(mq_in, (char *)&msg, MSGSZ, NULL);

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

            ServerMsg bye_msg;
            memset(&bye_msg, 0, sizeof(bye_msg));
            bye_msg.bye = 1;
            mq_send(mq_out, (char *)&bye_msg, sizeof(bye_msg), 0);
            mq_send(mq_out, (char *)&bye_msg, sizeof(bye_msg), 0);
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

        ServerMsg reply;
        memset(&reply, 0, sizeof(reply));
        strncpy(reply.status, status, sizeof(reply.status) - 1);
        reply.bye = 0;

        mq_send(mq_out, (char *)&reply, sizeof(reply), 0);
        mq_send(mq_out, (char *)&reply, sizeof(reply), 0);
    }

    printf("[SERVER] Cleaning up message queue...\n");
    mq_close(mq_in);
    mq_close(mq_out);
    mq_unlink(MQ_DATA);
    mq_unlink(MQ_STATUS);
    printf("[SERVER] Done.\n");

    return 0;
}
