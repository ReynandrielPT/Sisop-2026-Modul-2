#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

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

static mqd_t   mq_out, mq_in;
static int     my_id;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int    done  = 0;

static void *listener(void *arg) {
    (void)arg;
    ServerMsg msg;

    while (!done) {
        memset(&msg, 0, sizeof(msg));
        if (mq_receive(mq_in, (char *)&msg, MSGSZ, NULL) < 0) {
            if (done) break;
            continue;
        }

        pthread_mutex_lock(&mutex);
        if (msg.bye) {
            done = 1;
            printf("\n[INFO] Another sensor has exited\n");
            printf("[INFO] Cancelling current input...\n");
            printf("[INFO] System shutting down...\n");
            fflush(stdout);
            pthread_mutex_unlock(&mutex);
            break;
        }
        printf("\n[INFO] Current city status: %s\n\n", msg.status);
        printf("Masukkan data:\n");
        fflush(stdout);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

static int ok_loc(char loc, int sid) {
    if (sid == 1) return (loc == 'A' || loc == 'B');
    if (sid == 2) return (loc == 'C' || loc == 'D');
    return 0;
}

static int ok_st(char st) {
    return (st == 'L' || st == 'H');
}

int main(void) {
    mq_out = mq_open(MQ_DATA,   O_WRONLY);
    mq_in  = mq_open(MQ_STATUS, O_RDONLY);

    if (mq_out == (mqd_t)-1 || mq_in == (mqd_t)-1) {
        perror("mq_open");
        printf("Is the server running?\n");
        return 1;
    }

    SensorMsg reg;
    memset(&reg, 0, sizeof(reg));
    reg.type = T_REG;
    mq_send(mq_out, (char *)&reg, sizeof(reg), 0);

    ServerMsg ack;
    memset(&ack, 0, sizeof(ack));
    mq_receive(mq_in, (char *)&ack, MSGSZ, NULL);

    if (ack.bye) {
        printf("[INFO] System shutting down...\n");
        mq_close(mq_out);
        mq_close(mq_in);
        return 0;
    }

    my_id = ack.id;
    printf("[SENSOR %d] Connected as Sensor %d\n\n", my_id, my_id);

    pthread_t tid;
    pthread_create(&tid, NULL, listener, NULL);

    char input[128];
    while (!done) {
        printf("Masukkan data:\n");
        fflush(stdout);

        int count = 0;
        SensorMsg batch[2];
        memset(batch, 0, sizeof(batch));

        while (count < 2 && !done) {
            if (fgets(input, sizeof(input), stdin) == NULL) {
                usleep(100000);
                continue;
            }

            int n = strlen(input);
            while (n > 0 && (input[n-1] == '\n' || input[n-1] == '\r')) input[--n] = '\0';

            if (strcmp(input, "exit") == 0) {
                pthread_mutex_lock(&mutex);
                printf("[SENSOR] Sending exit signal...\n");
                fflush(stdout);
                pthread_mutex_unlock(&mutex);

                SensorMsg ex;
                memset(&ex, 0, sizeof(ex));
                ex.type = T_EXIT;
                ex.id   = my_id;
                mq_send(mq_out, (char *)&ex, sizeof(ex), 0);

                done = 1;

                pthread_mutex_lock(&mutex);
                printf("[SENSOR] Shutting down...\n");
                fflush(stdout);
                pthread_mutex_unlock(&mutex);
                goto cleanup;
            }

            int sid;
            char loc, st;
            if (sscanf(input, "%d %c %c", &sid, &loc, &st) != 3) {
                printf("Input tidak valid. Format: <ID_SENSOR> <LOKASI> <STATUS>\n");
                continue;
            }
            if (sid != my_id) {
                printf("ID sensor tidak sesuai. Anda adalah Sensor %d.\n", my_id);
                continue;
            }
            if (!ok_loc(loc, my_id)) {
                printf("Lokasi tidak valid untuk Sensor %d. Gunakan %s.\n",
                       my_id, my_id == 1 ? "A atau B" : "C atau D");
                continue;
            }
            if (!ok_st(st)) {
                printf("Status tidak valid. Gunakan L atau H.\n");
                continue;
            }

            batch[count].type = T_DATA;
            batch[count].id   = sid;
            batch[count].loc  = loc;
            batch[count].st   = st;
            count++;
        }

        if (done) break;

        for (int i = 0; i < count; i++)
            mq_send(mq_out, (char *)&batch[i], sizeof(batch[i]), 0);

        printf("\n");
    }

cleanup:
    pthread_join(tid, NULL);
    mq_close(mq_out);
    mq_close(mq_in);
    return 0;
}
