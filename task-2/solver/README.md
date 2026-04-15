# Pembahasan Task 2 _(Smart Traffic Controller / STC)_

## Kompilasi dan Cara Menjalankan

```
gcc server.c -o server
gcc sensor.c -o sensor -lpthread
```

```
./server            # terminal 1
./sensor            # terminal 2 (Sensor 1)
./sensor            # terminal 3 (Sensor 2)
```

---

## Desain Antrean Pesan

Program menggunakan **2 message queue**:

| Nama         | Arah            | Fungsi                            |
| ------------ | --------------- | --------------------------------- |
| `/stc_s2srv` | sensor ? server | Semua pesan dari sensor ke server |
| `/stc_srv2s` | server ? sensor | Semua pesan dari server ke sensor |

Karena kedua sensor berbagi queue yang sama, server membedakan pesan berdasarkan field `type` di dalam struct `SensorMsg`.

### Tipe Pesan

| Konstanta | Nilai | Makna                            |
| --------- | ----- | -------------------------------- |
| `T_REG`   | 0     | Sensor mendaftar ke server       |
| `T_DATA`  | 1     | Sensor mengirim data lalu lintas |
| `T_EXIT`  | 2     | Sensor mengirim sinyal keluar    |

### Struct Pesan

```
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
```

---

## a. Setup Koneksi _(Connection Setup)_

### Soal

Buat `server.c` sebagai pusat kontrol dan `sensor.c` sebagai client sensor. Komunikasi menggunakan **2 Message Queue** (satu arah masing-masing). Sensor pertama yang terhubung menjadi Sensor 1, sensor kedua menjadi Sensor 2.

### Penyelesaian

- Code Lengkap (server.c bagian koneksi):

```
#define MQ_DATA   "/stc_data"
#define MQ_STATUS "/stc_status"
#define MSGSZ    256

#define T_REG  0
#define T_DATA 1
#define T_EXIT 2

int main(void) {
    struct mq_attr attr = {0, 20, MSGSZ, 0};

    mq_unlink(MQ_DATA);
    mq_unlink(MQ_STATUS);

    mqd_t mq_in  = mq_open(MQ_DATA,   O_CREAT | O_RDONLY, 0666, &attr);
    mqd_t mq_out = mq_open(MQ_STATUS, O_CREAT | O_WRONLY, 0666, &attr);

    printf("[SERVER] Waiting for sensors...
");

    int connected = 0;
    while (connected < 2) {
        SensorMsg msg;
        memset(&msg, 0, sizeof(msg));
        mq_receive(mq_in, (char *)&msg, MSGSZ, NULL);

        if (msg.type == T_REG) {
            connected++;
            printf("[SERVER] Sensor %d connected
", connected);

            ServerMsg reply;
            memset(&reply, 0, sizeof(reply));
            reply.id  = connected;
            reply.bye = 0;
            mq_send(mq_out, (char *)&reply, sizeof(reply), 0);
        }
    }
    printf("[SERVER] System ready!

");
}
```

- Code Lengkap (sensor.c bagian koneksi):

```
static mqd_t mq_out, mq_in;
static int   my_id;

int main(void) {
    mq_out = mq_open(MQ_S2SRV, O_WRONLY);
    mq_in  = mq_open(MQ_SRV2S, O_RDONLY);

    SensorMsg reg;
    memset(&reg, 0, sizeof(reg));
    reg.type = T_REG;
    mq_send(mq_out, (char *)&reg, sizeof(reg), 0);

    ServerMsg ack;
    memset(&ack, 0, sizeof(ack));
    mq_receive(mq_in, (char *)&ack, MSGSZ, NULL);

    my_id = ack.id;
    printf("[SENSOR %d] Connected as Sensor %d

", my_id, my_id);
}
```

Penjelasan:

```
struct mq_attr attr = {0, 20, MSGSZ, 0};
```

Antrean dibuat dengan kapasitas 20 pesan dan ukuran 256 byte. Kapasitas lebih besar karena kedua sensor berbagi satu queue; pesan bisa menumpuk.

```
if (msg.type == T_REG) {
    connected++;
    reply.id = connected;
    mq_send(mq_out, (char *)&reply, sizeof(reply), 0);
}
```

Server menetapkan ID sensor berdasarkan urutan registrasi. Sensor pertama mendapat `id = 1`, sensor kedua `id = 2`, langsung dikirim balik ke `MQ_STATUS`.

```
reg.type = T_REG;
mq_send(mq_out, (char *)&reg, sizeof(reg), 0);
ServerMsg ack;
mq_receive(mq_in, (char *)&ack, MSGSZ, NULL);
my_id = ack.id;
```

Sensor menulis ke `MQ_DATA` untuk mendaftar, lalu menunggu balasan `ack` dari `MQ_STATUS` secara blocking. Ini memungkinkan server memproses registrasi secara berurutan tanpa race condition.

---

## b. Input Data Sensor _(Input Sensor Data)_

### Soal

Setiap sensor mengirimkan data 2 lokasi per siklus secara terus-menerus. Format: `<ID_SENSOR> <LOKASI> <STATUS>`. Sistem berhenti bila sensor mengirim `exit`.

### Penyelesaian

- Code Lengkap (sensor.c bagian input):

```
char input[128];
while (!done) {
    printf("Masukkan data:
");
    fflush(stdout);

    int count = 0;
    SensorMsg batch[2];
    memset(batch, 0, sizeof(batch));

    while (count < 2 && !done) {
        if (fgets(input, sizeof(input), stdin) == NULL) {
            usleep(100000); continue;
        }

        int n = strlen(input);
        while (n > 0 && (input[n-1]=='
'||input[n-1]=='\r')) input[--n]='\0';

        if (strcmp(input, "exit") == 0) {
            printf("[SENSOR] Sending exit signal...
");

            SensorMsg ex;
            memset(&ex, 0, sizeof(ex));
            ex.type = T_EXIT;
            ex.id   = my_id;
            mq_send(mq_out, (char *)&ex, sizeof(ex), 0);
            done = 1;

            printf("[SENSOR] Shutting down...
");
            goto cleanup;
        }

        int sid; char loc, st;
        if (sscanf(input, "%d %c %c", &sid, &loc, &st) != 3) {
            printf("Input tidak valid. Format: <ID_SENSOR> <LOKASI> <STATUS>
");
            continue;
        }
        if (sid != my_id) {
            printf("ID sensor tidak sesuai. Anda adalah Sensor %d.
", my_id);
            continue;
        }
        if (!ok_loc(loc, my_id)) {
            printf("Lokasi tidak valid untuk Sensor %d. Gunakan %s.
",
                   my_id, my_id == 1 ? "A atau B" : "C atau D");
            continue;
        }
        if (!ok_st(st)) {
            printf("Status tidak valid. Gunakan L atau H.
");
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
    printf("
");
}
cleanup:
```

Penjelasan:

```
static int ok_loc(char loc, int sid) {
    if (sid == 1) return (loc == 'A' || loc == 'B');
    if (sid == 2) return (loc == 'C' || loc == 'D');
    return 0;
}
```

Sensor 1 hanya boleh melaporkan A dan B, Sensor 2 hanya C dan D sesuai alokasi soal.

```
SensorMsg batch[2];
for (int i = 0; i < count; i++)
    mq_send(mq_out, (char *)&batch[i], sizeof(batch[i]), 0);
```

Data dikumpulkan dulu dalam `batch`, baru dikirim setelah dua input valid terkumpul agar server menerima data per siklus secara utuh.

```
ex.type = T_EXIT; ex.id = my_id;
mq_send(mq_out, (char *)&ex, sizeof(ex), 0);
done = 1;
goto cleanup;
```

Sinyal exit dikirim dengan tipe `T_EXIT`. Flag `done = 1` menghentikan semua loop. `goto cleanup` langsung menuju `pthread_join` dan penutupan queue.

---

## c. Pemrosesan Data pada Server _(Server Data Processing)_

### Soal

Server menunggu tepat 4 data per siklus, menampilkan kondisi tiap lokasi, menghitung H dan L, lalu menentukan status kota. Menangani sinyal exit di tengah siklus.

### Penyelesaian

- Code Lengkap (server.c bagian pemrosesan):

```
char traffic[4];

while (1) {
    int cnt = 0, exited = 0;
    memset(traffic, '?', sizeof(traffic));

    while (cnt < 4) {
        SensorMsg msg;
        memset(&msg, 0, sizeof(msg));
        mq_receive(mq_in, (char *)&msg, MSGSZ, NULL);

        if (msg.type == T_EXIT) { exited = 1; break; }

        if (msg.type == T_DATA) {
            int idx = msg.loc - 'A';
            if (idx >= 0 && idx < 4)
                traffic[idx] = msg.st;
            cnt++;
        }
    }

    if (exited) {
        printf("[SERVER] Exit signal received
");
        printf("[SERVER] Shutting down system...
");
        ServerMsg bye_msg; memset(&bye_msg, 0, sizeof(bye_msg));
        bye_msg.bye = 1;
        mq_send(mq_out, (char *)&bye_msg, sizeof(bye_msg), 0);
        mq_send(mq_out, (char *)&bye_msg, sizeof(bye_msg), 0);
        break;
    }

    printf("[SERVER] Received data:
");
    printf("  A: %c
  B: %c
  C: %c
  D: %c
",
           traffic[0], traffic[1], traffic[2], traffic[3]);

    int high = 0, low = 0;
    for (int i = 0; i < 4; i++) {
        if      (traffic[i] == 'H') high++;
        else if (traffic[i] == 'L') low++;
    }

    printf("
[SERVER] Summary:
");
    printf("  High Traffic: %d
  Low Traffic : %d
", high, low);

    const char *status;
    if      (high >= 3) status = "MACET TOTAL";
    else if (high == 2) status = "PADAT";
    else                status = "LANCAR";

    printf("
[SERVER] City Status: %s

", status);

    ServerMsg reply; memset(&reply, 0, sizeof(reply));
    strncpy(reply.status, status, sizeof(reply.status) - 1);
    mq_send(mq_out, (char *)&reply, sizeof(reply), 0);
    mq_send(mq_out, (char *)&reply, sizeof(reply), 0);
}
```

Penjelasan:

```
int idx = msg.loc - 'A';
traffic[idx] = msg.st;
```

Lokasi `A`?`D` dipetakan ke indeks `0`?`3`. Array `traffic[4]` menyimpan status tiap lokasi sehingga tampilan selalu berurutan A, B, C, D meski data datang dari dua sensor berbeda.

```
if (msg.type == T_EXIT) { exited = 1; break; }
```

Server tidak menunggu sisa data jika sinyal exit diterima di tengah siklus. Loop langsung keluar dan masuk ke blok shutdown.

```
bye_msg.bye = 1;
mq_send(mq_out, (char *)&bye_msg, sizeof(bye_msg), 0);
mq_send(mq_out, (char *)&bye_msg, sizeof(bye_msg), 0);
```

Shutdown dikirim dua kali (satu per sensor) karena kedua sensor membaca dari queue yang sama.

---

## d. Penentuan Status _(Status Determination)_

### Soal

`=3 H` ? MACET TOTAL, `2 H` ? PADAT, `=1 H` ? LANCAR.

### Penyelesaian

```
const char *status;
if      (high >= 3) status = "MACET TOTAL";
else if (high == 2) status = "PADAT";
else                status = "LANCAR";
```

Penjelasan:
`status` adalah pointer ke string literal konstan lalu disalin ke `reply.status` dengan `strncpy` agar bisa dikirim lewat message queue.

---

## e. Pengelolaan Thread dan Sinkronisasi _(Thread Management and Synchronization)_

### Soal

`sensor.c` wajib menggunakan thread dan mutex. Thread utama menangani input, thread `listener` mendengarkan pesan server secara asinkron.

### Penyelesaian

- Code Lengkap (sensor.c bagian threading):

```
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
            printf("
[INFO] Another sensor has exited
");
            printf("[INFO] Cancelling current input...
");
            printf("[INFO] System shutting down...
");
            fflush(stdout);
            pthread_mutex_unlock(&mutex);
            break;
        }
        printf("
[INFO] Current city status: %s

", msg.status);
        printf("Masukkan data:
");
        fflush(stdout);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Di main():
pthread_t tid;
pthread_create(&tid, NULL, listener, NULL);
// ... input loop ...
cleanup:
pthread_join(tid, NULL);
mq_close(mq_out);
mq_close(mq_in);
```

Penjelasan:

```
static volatile int done = 0;
```

`volatile` memastikan perubahan dari satu thread langsung terlihat di thread lain. `done` diset `1` oleh listener saat menerima `bye`, atau oleh main thread saat user mengetik `exit`.

```
pthread_mutex_lock(&mutex);
printf("
[INFO] Current city status: %s

", msg.status);
pthread_mutex_unlock(&mutex);
```

Semua operasi `printf` dilindungi mutex agar output dari listener dan main thread tidak saling tindih.

```
if (msg.bye) {
    done = 1;
    pthread_mutex_unlock(&mutex);
    break;
}
```

Saat menerima `bye` dari server, listener set `done = 1`, cetak pesan info, lalu keluar dari loop. Main thread membaca flag `done` dan berhenti dari input loop.

```
pthread_join(tid, NULL);
```

Main thread menunggu listener selesai sebelum menutup queue agar listener tidak membaca dari queue yang sudah ditutup.

---

## f. Pembersihan Resource _(Resource Cleanup)_

### Soal

Server wajib menghapus semua message queue setelah sistem dihentikan.

### Penyelesaian

```
printf("[SERVER] Cleaning up message queue...
");
mq_close(mq_in);
mq_close(mq_out);
mq_unlink(MQ_S2SRV);
mq_unlink(MQ_SRV2S);
printf("[SERVER] Done.
");
```

Penjelasan:
`mq_close()` menutup file descriptor queue pada proses ini. `mq_unlink()` menghapus queue dari sistem (`/dev/mqueue/`). Keduanya harus dipanggil � `mq_close` saja tidak menghapus queue dari sistem.
