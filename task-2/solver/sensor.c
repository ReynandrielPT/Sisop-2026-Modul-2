#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

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

static int     q_data = -1, q_status = -1;
static int     my_id;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int    done  = 0;

static key_t key_data(void) {
    return ftok(".", 'D');
}

static key_t key_status(void) {
    return ftok(".", 'S');
}

static void *listener(void *arg) {
    (void)arg;
    StatusPacket pkt;

    while (!done) {
        memset(&pkt, 0, sizeof(pkt));
        if (msgrcv(q_status, &pkt, sizeof(pkt.body), my_id, IPC_NOWAIT) < 0) {
            if (errno == ENOMSG) {
                usleep(50000);
                continue;
            }
            if (done) break;
            continue;
        }

        pthread_mutex_lock(&mutex);
        if (pkt.body.bye) {
            done = 1;
            printf("\n[INFO] Another sensor has exited\n");
            printf("[INFO] Cancelling current input...\n");
            printf("[INFO] System shutting down...\n");
            fflush(stdout);
            pthread_mutex_unlock(&mutex);
            break;
        }
        printf("\n[INFO] Current city status: %s\n\n", pkt.body.status);
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

static void set_stdin_nonblocking(void) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
}

int main(void) {
    key_t k_data = key_data();
    key_t k_status = key_status();

    if (k_data == (key_t)-1 || k_status == (key_t)-1) {
        perror("ftok");
        return 1;
    }

    q_data = msgget(k_data, 0666);
    q_status = msgget(k_status, 0666);

    if (q_data < 0 || q_status < 0) {
        perror("msgget");
        printf("Is the server running?\n");
        return 1;
    }

    DataPacket reg;
    memset(&reg, 0, sizeof(reg));
    reg.mtype = 1;
    reg.body.type = T_REG;
    reg.body.pid = (int)getpid();
    msgsnd(q_data, &reg, sizeof(reg.body), 0);

    StatusPacket ack;
    memset(&ack, 0, sizeof(ack));
    msgrcv(q_status, &ack, sizeof(ack.body), reg.body.pid, 0);

    if (ack.body.bye) {
        printf("[INFO] System shutting down...\n");
        return 0;
    }

    my_id = ack.body.id;
    printf("[SENSOR %d] Connected as Sensor %d\n\n", my_id, my_id);

    pthread_t tid;
    pthread_create(&tid, NULL, listener, NULL);

    set_stdin_nonblocking();

    char input[128];
    while (!done) {
        printf("Masukkan data:\n");
        fflush(stdout);

        int count = 0;
        SensorMsg batch[2];
        memset(batch, 0, sizeof(batch));

        while (count < 2 && !done) {
            if (fgets(input, sizeof(input), stdin) == NULL) {
                if (errno != EAGAIN && errno != EWOULDBLOCK && !done) {
                    clearerr(stdin);
                }
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

                DataPacket ex;
                memset(&ex, 0, sizeof(ex));
                ex.mtype = my_id;
                ex.body.type = T_EXIT;
                ex.body.id   = my_id;
                ex.body.pid  = (int)getpid();
                msgsnd(q_data, &ex, sizeof(ex.body), 0);

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

            loc = (char)toupper((unsigned char)loc);
            st  = (char)toupper((unsigned char)st);

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
            batch[count].pid  = (int)getpid();
            batch[count].loc  = loc;
            batch[count].st   = st;
            count++;
        }

        if (done) break;

        for (int i = 0; i < count; i++) {
            DataPacket pkt;
            memset(&pkt, 0, sizeof(pkt));
            pkt.mtype = my_id;
            pkt.body = batch[i];
            msgsnd(q_data, &pkt, sizeof(pkt.body), 0);
        }

        printf("\n");
    }

cleanup:
    pthread_join(tid, NULL);
    return 0;
}
