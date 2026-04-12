# Pembahasan Task 3 _(Simulasi Pertempuran Laut / Naval Battle Simulation)_

## Kompilasi dan Cara Menjalankan

Kompilasi server:
```bash
gcc server.c -o server -lrt
```

Kompilasi player:
```bash
gcc player.c -o player -lrt -lpthread
```

Jalankan server terlebih dahulu di terminal pertama:
```bash
./server
```

Jalankan player di terminal kedua dan ketiga:
```bash
./player
```

---

## a. Persiapan Koneksi dan Antrean Pesan _(Connection Setup and Message Queues)_
### Soal
Buatlah program `server.c` yang berperan sebagai **Game Master** dan program `player.c` sebagai klien pemain interaktif. Server harus membuat dan mengelola **POSIX Message Queue** untuk komunikasi. Dibutuhkan 1 queue publik untuk menerima koneksi pemain baru dan **4 queue privat** untuk komunikasi dua arah saat gameplay: 2 queue untuk menerima pesan dari setiap pemain, dan 2 queue untuk mengirimkan respons ke setiap pemain. Player dijalankan sederhana tanpa argumen tambahan: `./player`. Program pertama yang terhubung akan ditugaskan sebagai Player 1, dan yang kedua sebagai Player 2.

### Penyelesaian
- Code Lengkap (server.c bagian koneksi):
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MQ_BS_JOIN    "/mq_bs_join"
#define MQ_P1_TO_SRV  "/mq_bs_p1_srv"
#define MQ_P2_TO_SRV  "/mq_bs_p2_srv"
#define MQ_SRV_TO_P1  "/mq_bs_srv_p1"
#define MQ_SRV_TO_P2  "/mq_bs_srv_p2"
#define MSG_SIZE      512

static mqd_t mq_in[2], mq_out[2];

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
}
```

- Code Lengkap (player.c bagian koneksi):
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>

#define MQ_BS_JOIN    "/mq_bs_join"
#define MSG_SIZE      512

static mqd_t mq_out, mq_in;
static int player_id;

int main(void) {
    printf("[PEMAIN] Menghubungkan ke server...\n");

    pid_t pid = getpid();
    char temp_q[128];
    snprintf(temp_q, sizeof(temp_q), "/mq_bs_temp_%d", pid);

    struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10,
                            .mq_msgsize = MSG_SIZE, .mq_curmsgs = 0 };
    mqd_t mq_temp = mq_open(temp_q, O_CREAT | O_RDONLY, 0666, &attr);

    mqd_t mq_join = mq_open(MQ_BS_JOIN, O_WRONLY);

    char join_msg[MSG_SIZE];
    snprintf(join_msg, sizeof(join_msg), "CONNECT %s", temp_q);
    mq_send(mq_join, join_msg, strlen(join_msg) + 1, 0);
    mq_close(mq_join);

    char buf[MSG_SIZE];
    memset(buf, 0, MSG_SIZE);
    mq_receive(mq_temp, buf, MSG_SIZE, NULL);
    player_id = atoi(buf);
    mq_close(mq_temp);
    mq_unlink(temp_q);

    printf("[PEMAIN %d] Berhasil terhubung!\n", player_id);
}
```

Penjelasan:
```c
#include <mqueue.h>
```
Pustaka `mqueue.h` merupakan pustaka POSIX Message Queue yang menyediakan API untuk komunikasi antar proses (IPC). Pustaka ini wajib digunakan dan saat kompilasi harus disertakan flag `-lrt`.

```c
#define MQ_BS_JOIN    "/mq_bs_join"
#define MQ_P1_TO_SRV  "/mq_bs_p1_srv"
#define MQ_P2_TO_SRV  "/mq_bs_p2_srv"
#define MQ_SRV_TO_P1  "/mq_bs_srv_p1"
#define MQ_SRV_TO_P2  "/mq_bs_srv_p2"
```
Mendefinisikan 5 nama antrean pesan POSIX. `MQ_BS_JOIN` adalah antrean publik untuk player mendaftar. Setelah mendaftar, komunikasi gameplay diteruskan lewat jalur privat: `MQ_P1_TO_SRV` dan `MQ_SRV_TO_P1` untuk Player 1, serta `MQ_P2_TO_SRV` dan `MQ_SRV_TO_P2` untuk Player 2. Nama-nama ini akan muncul di `/dev/mqueue/` pada sistem Linux.

```c
struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10, .mq_msgsize = MSG_SIZE, .mq_curmsgs = 0 };
```
Mendeklarasikan atribut antrean. `.mq_maxmsg = 10` berarti antrean bisa menampung maksimal 10 pesan sebelum penuh. `.mq_msgsize = MSG_SIZE` berarti setiap pesan berukuran maksimal 512 byte.

```c
mq_unlink(MQ_BS_JOIN);
mq_unlink(MQ_P1_TO_SRV); mq_unlink(MQ_P2_TO_SRV);
mq_unlink(MQ_SRV_TO_P1); mq_unlink(MQ_SRV_TO_P2);
```
Me-*reset* antrean yang mungkin masih menggantung dari eksekusi sebelumnya. Jika game dihentikan paksa, sistem operasi tidak secara otomatis menghapus Message Queue di `/dev/mqueue/`, sehingga pesan lama bisa tercampur. Fungsi `mq_unlink()` menghapus antrean tersebut.

```c
mq_in[0]  = mq_open(MQ_P1_TO_SRV, O_CREAT | O_RDONLY, 0666, &attr);
```
Membuka (atau membuat jika belum ada) antrean dengan `mq_open()`. Flag `O_CREAT` memerintahkan OS untuk membuat antrean baru, `O_RDONLY` artinya server hanya membaca dari antrean ini (player yang menulis). Permission `0666` memberikan akses baca-tulis untuk semua pengguna.

```c
for (int i = 0; i < 2; i++) {
    mq_receive(mq_join, buf, MSG_SIZE, NULL);
```
Server melakukan *loop* menunggu 2 player bergabung. Fungsi `mq_receive()` bersifat *blocking* — artinya program akan berhenti di baris ini sampai ada pesan masuk di `mq_join`.

```c
if (strncmp(buf, "CONNECT ", 8) == 0) {
```
Server hanya merespons pesan yang diawali dengan `CONNECT`. Jika pesan tidak valid, `i--` memaksa iterasi diulang sehingga server terus menunggu sampai player yang sah terhubung.

```c
mqd_t mq_temp = mq_open(temp_q, O_WRONLY);
snprintf(reply, sizeof(reply), "%d", i + 1);
mq_send(mq_temp, reply, strlen(reply) + 1, 0);
mq_close(mq_temp);
```
Server mengekstrak nama antrean sementara milik klien dari pesan `CONNECT`, membukanya, lalu mengirim balik ID pemain (`"1"` atau `"2"`). Setelah itu antrean sementara langsung ditutup karena komunikasi selanjutnya menggunakan jalur privat yang sudah disiapkan.

```c
// Di player.c:
snprintf(temp_q, sizeof(temp_q), "/mq_bs_temp_%d", pid);
```
Setiap player membuat antrean sementara unik berdasarkan PID prosesnya. Ini memastikan dua player yang dijalankan bersamaan tidak bertabrakan. Player mengirim `"CONNECT /mq_bs_temp_12345"` ke server, lalu menunggu balasan berupa ID di antrean sementara tersebut.

```c
mq_close(mq_join);
mq_unlink(MQ_BS_JOIN);
```
Setelah kedua player terhubung, antrean join ditutup dan dihapus karena tidak lagi dibutuhkan. Ini juga mencegah proses ketiga ikut bergabung ke sesi yang sudah berjalan.

---

## b. Penempatan Armada _(Fleet Placement)_
### Soal
Setelah kedua pemain terhubung, masing-masing pemain menempatkan 2 kapalnya. Program meminta pemain memasukkan koordinat 1 petak per kapal. Koordinat harus valid (dalam batas papan) dan tidak menimpa kapal yang sudah ada.

### Penyelesaian
- Code Lengkap (server.c bagian penempatan):
```c
#define ROWS          4
#define COLS          4
#define MAX_SHIPS     2
#define CELL_EMPTY   '.'
#define CELL_SHIP    'S'

typedef struct {
    int  r, c;
    int  sunk;
    int  index;
} Ship;

typedef struct {
    char board[ROWS][COLS];
    char hit_board[ROWS][COLS];
    Ship ships[MAX_SHIPS];
    int  num_ships;
    int  ships_remaining;
    int  connected;
} Player;

static int place_ship(Player *p, int idx, int r, int c) {
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return -3;
    if (p->board[r][c] != CELL_EMPTY) return -4;
    Ship *s = &p->ships[p->num_ships];
    s->r = r;  s->c = c;  s->sunk = 0;  s->index = idx;
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
}
```

- Code Lengkap (player.c bagian penempatan):
```c
/* Show initial empty boards */
printf("    Papan Lawan\n");
printf("  A B C D\n");
for (int r = 0; r < 4; r++) {
    printf("%d|", r);
    for (int c = 0; c < 4; c++)
        printf("?|");
    printf("\n");
}
printf("\n");
printf("    Papan Anda\n");
printf("  A B C D\n");
for (int r = 0; r < 4; r++) {
    printf("%d|", r);
    for (int c = 0; c < 4; c++)
        printf(" |");
    printf("\n");
}
printf("\n");

printf("Anda akan menempatkan 2 kapal (masing-masing 1 petak).\n\n");

char input[100];
for (int i = 1; i <= 2; i++) {
    while (1) {
        printf("Tempatkan Kapal %d:\n> ", i);
        fgets(input, sizeof(input), stdin);
        remove_newline(input);

        char place_cmd[MSG_SIZE];
        snprintf(place_cmd, sizeof(place_cmd), "PLACE %s", input);
        mq_send(mq_out, place_cmd, strlen(place_cmd)+1, 0);

        mq_receive(mq_in, buf, MSG_SIZE, NULL);
        if (strncmp(buf, "PLACE_ERR", 9) == 0) {
            printf("%s\n", buf + 10);
        } else if (strncmp(buf, "PLACE_OK", 8) == 0) {
            printf("%s\n", buf + 9);
            break;
        }
    }
}
printf("\n[INFO] Menunggu lawan menyelesaikan penempatan...\n");
```

Penjelasan:
```c
typedef struct {
    char board[ROWS][COLS];
    char hit_board[ROWS][COLS];
    Ship ships[MAX_SHIPS];
    int  num_ships;
    int  ships_remaining;
    int  connected;
} Player;
```
Struktur `Player` menyimpan seluruh state pemain. `board` adalah matriks 4x4 yang menyimpan posisi kapal (`S`) dan petak kosong (`.`). `hit_board` mencatat riwayat tembakan lawan pada papan pemain — `X` untuk kena, ` ` untuk meleset. `ships` menyimpan detail tiap kapal (posisi, status tenggelam). `ships_remaining` digunakan untuk menentukan kapan pemain kalah.

```c
static int col_to_idx(char c) {
    if (c >= 'a' && c <= 'd') return c - 'a';
    if (c >= 'A' && c <= 'D') return c - 'A';
    return -1;
}
```
Fungsi utilitas untuk mengubah kolom huruf (`A`-`D`) menjadi indeks angka (`0`-`3`). Mendukung huruf besar maupun kecil sehingga pemain bisa mengetik `0a` maupun `0A`.

```c
static int place_ship(Player *p, int idx, int r, int c) {
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return -3;
    if (p->board[r][c] != CELL_EMPTY) return -4;
```
Fungsi `place_ship()` memvalidasi koordinat sebelum menetapkan kapal. Return `-3` berarti koordinat di luar batas papan, dan `-4` berarti petak sudah ditempati kapal lain. Jika valid, kapal disiapkan pada array `ships` dan simbol `S` ditanam di `board[r][c]`.

```c
if (sscanf(buf, "PLACE %s", coord_str) != 1) {
    srv_send(pid, "PLACE_ERR Format tidak valid.");
    continue;
}
```
Server menerima pesan mentah dari player (misalnya `"PLACE 0A"`). Fungsi `sscanf` mengekstrak bagian koordinat. Jika format tidak sesuai, server mengirim pesan error dan mengulang iterasi `while` agar player bisa mencoba lagi.

```c
// Di player.c:
snprintf(place_cmd, sizeof(place_cmd), "PLACE %s", input);
mq_send(mq_out, place_cmd, strlen(place_cmd)+1, 0);
```
Player membaca input dari `stdin` menggunakan `fgets()`, membentuk pesan `"PLACE 0A"`, lalu mengirimnya ke server via antrean `mq_out`. Setelah itu player menunggu respons `PLACE_OK` atau `PLACE_ERR` dari server.

---

## c. Giliran Bermain dan Penembakan _(Turn System and Firing)_
### Soal
Setelah setup selesai, server menentukan pemain pertama secara acak. Setiap giliran, pemain melihat papan lawan (atas) dan papan sendiri (bawah), lalu mengetikkan koordinat target. Server memproses hasil tembakan dan mengirimkan notifikasi ke kedua pemain. Pemain dapat menembak petak yang sebelumnya sudah pernah ditembak, namun hasilnya selalu dianggap `MELESET`.

### Penyelesaian
- Code Lengkap (server.c bagian penembakan):
```c
static void process_fire(int shooter_pid) {
    int target_pid = 1 - shooter_pid;
    Player *target  = &players[target_pid];
    char buf[MSG_SIZE];

    while (1) {
        srv_recv(shooter_pid, buf);
        char coord_str[8];
        if (sscanf(buf, "FIRE %s", coord_str) != 1) {
            srv_send(shooter_pid, "FIRE_ERR Format tidak valid.");
            continue;
        }
        int r = coord_str[0] - '0';
        int c = col_to_idx(coord_str[1]);
        if (r < 0 || r >= ROWS || c < 0 || c >= COLS) {
            srv_send(shooter_pid, "FIRE_ERR Koordinat di luar batas!");
            continue;
        }

        int hit = 0, sunk = 0;
        /* Already shot — always miss */
        if (target->hit_board[r][c] == CELL_HIT || target->hit_board[r][c] == CELL_MISS) {
            hit = 0;
        } else if (target->board[r][c] == CELL_SHIP) {
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

        printf("[GILIRAN] Pemain %d menembak %s: %s\n", shooter_pid + 1, coord_str, hit ? "KENA" : "MELESET");
        if (sunk)
            printf("[TENGGELAM] Pemain %d menenggelamkan Kapal Pemain %d!\n", shooter_pid + 1, target_pid + 1);

        char res_msg[MSG_SIZE];
        if (hit)
            snprintf(res_msg, MSG_SIZE, "FIRE_RES %s KENA KAPAL, SISA %d KAPAL LAGI", coord_str, target->ships_remaining);
        else
            snprintf(res_msg, MSG_SIZE, "FIRE_RES %s MELESET, SISA %d KAPAL LAGI", coord_str, target->ships_remaining);

        snprintf(pending_opp_msg[target_pid], MSG_SIZE, "OPP_FIRE %s: %s, SISA %d KAPAL LAGI",
                 coord_str, hit ? "KENA KAPAL" : "MELESET", target->ships_remaining);
        has_pending_opp_msg[target_pid] = 1;
        srv_send(shooter_pid, res_msg);
        return;
    }
}

static void run_game(void) {
    srand((unsigned)time(NULL));
    int current = rand() % 2;
    printf("[SERVER] Dipilih secara acak: Pemain %d bermain duluan.\n", current + 1);

    while (1) {
        int other = 1 - current;

        if (has_pending_opp_msg[current]) {
            srv_send(current, pending_opp_msg[current]);
            has_pending_opp_msg[current] = 0;
            usleep(50000);
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
```

- Code Lengkap (player.c bagian menembak):
```c
// Dalam main loop setelah thread dibuat:
while (!game_over) {
    if (fgets(input, sizeof(input), stdin) != NULL) {
        remove_newline(input);
        pthread_mutex_lock(&console_mutex);
        int is_turn = my_turn;
        pthread_mutex_unlock(&console_mutex);

        if (is_turn && strlen(input) > 0) {
            pthread_mutex_lock(&console_mutex);
            if (strlen(input) < 2) {
                 printf("Format tidak valid!\nTarget: ");
                 fflush(stdout);
                 pthread_mutex_unlock(&console_mutex);
                 continue;
            }
            char fire_cmd[MSG_SIZE];
            snprintf(fire_cmd, sizeof(fire_cmd), "FIRE %s", input);
            mq_send(mq_out, fire_cmd, strlen(fire_cmd)+1, 0);
            my_turn = 0;
            pthread_mutex_unlock(&console_mutex);
        }
    }
}
```

Penjelasan:
```c
srand((unsigned)time(NULL));
int current = rand() % 2;
```
Server memilih pemain pertama secara acak menggunakan `rand() % 2`. Fungsi `srand()` menggunakan waktu saat ini sebagai seed agar hasil selalu berbeda setiap kali server dijalankan.

```c
build_turn_msg(current, turn_msg, sizeof(turn_msg));
srv_send(current, turn_msg);
```
Fungsi `build_turn_msg()` menyusun string yang berisi data papan pemain sendiri dan papan lawan. String ini dikirim ke player yang sedang mendapat giliran. Player kemudian mem-*parse* string ini untuk menampilkan kedua papan di terminal.

```c
if (target->hit_board[r][c] == CELL_HIT || target->hit_board[r][c] == CELL_MISS) {
    hit = 0;
}
```
Sebelum memproses tembakan baru, server mengecek apakah petak sudah pernah ditembak sebelumnya. Jika `hit_board` sudah berisi `CELL_HIT` atau `CELL_MISS`, tembakan ulang selalu dianggap meleset (`hit = 0`) tanpa mengubah state papan. Ini mencegah pemain mendapat hasil "KENA" pada kapal yang sudah tenggelam.

```c
if (target->board[r][c] == CELL_SHIP) {
    hit = 1;
    target->hit_board[r][c] = CELL_HIT;
```
Ketika koordinat tembakan mengenai petak yang berisi kapal (`CELL_SHIP` / `S`), server menandai `hit = 1` dan memperbarui `hit_board` lawan pada posisi tersebut menjadi `CELL_HIT` (`X`). Ini akan terlihat di papan "Papan Anda" milik lawan saat giliran berikutnya.

```c
} else {
    target->hit_board[r][c] = CELL_MISS;
}
```
Jika tembakan meleset (petak kosong), `hit_board` diisi dengan `CELL_MISS` (spasi ` `). Sehingga lawan bisa melihat di mana musuhnya sudah pernah menembak dan meleset.

```c
for (int i = 0; i < target->num_ships; i++) {
    if (target->ships[i].r == r && target->ships[i].c == c && !target->ships[i].sunk) {
        target->ships[i].sunk = 1;
        target->ships_remaining--;
        sunk = 1;
        break;
    }
}
```
Setelah mendeteksi `hit`, server mencari kapal mana yang terkena di array `ships`. Kapal yang terkena ditandai `sunk = 1` dan `ships_remaining` dikurangi satu. Karena setiap kapal hanya berukuran 1 petak, satu kali kena langsung tenggelam.

```c
if (players[other].ships_remaining == 0) {
    srv_send(current, "YOU_WIN");
    srv_send(other,   "YOU_LOSE");
```
Setiap selesai memproses tembakan, server mengecek apakah semua kapal lawan sudah tenggelam. Jika `ships_remaining == 0`, server mengirim pesan kemenangan ke penembak dan kekalahan ke lawan, lalu keluar dari game loop.

```c
snprintf(pending_opp_msg[target_pid], MSG_SIZE, "OPP_FIRE %s: %s, SISA %d KAPAL LAGI",
         coord_str, hit ? "KENA KAPAL" : "MELESET", target->ships_remaining);
has_pending_opp_msg[target_pid] = 1;
```
Setelah memproses tembakan, server menyimpan pesan notifikasi untuk lawan di buffer `pending_opp_msg`. Pesan ini tidak langsung dikirim — server menggunakan mekanisme *deferred delivery* agar pesan `OPP_FIRE` dikirim di awal giliran berikutnya milik lawan. Flag `has_pending_opp_msg` menandai bahwa ada pesan tertunda.

```c
if (has_pending_opp_msg[current]) {
    srv_send(current, pending_opp_msg[current]);
    has_pending_opp_msg[current] = 0;
    usleep(50000);
}
```
Di awal setiap giliran dalam `run_game()`, server mengecek apakah ada pesan `OPP_FIRE` tertunda untuk pemain yang akan bermain. Jika ada, pesan dikirim terlebih dahulu sebelum data papan. `usleep(50000)` memberi jeda 50ms agar pesan tercetak berurutan di terminal player.

```c
// Di player.c:
snprintf(fire_cmd, sizeof(fire_cmd), "FIRE %s", input);
mq_send(mq_out, fire_cmd, strlen(fire_cmd)+1, 0);
my_turn = 0;
```
Player membentuk pesan `"FIRE 2C"` dari input pengguna dan mengirimnya ke server. Variabel `my_turn` langsung di-set ke `0` agar player tidak bisa menembak lagi sebelum menerima giliran baru dari server.

---

## d. Pengelolaan Thread, Akhir Permainan, dan Pembersihan _(Thread Management, End of Game, and Cleanup)_
### Soal
Program `player.c` harus menggunakan **thread** dan **mutex** untuk mengelola komunikasi secara bersamaan. Thread utama menangani input, thread terpisah mendengarkan pesan dari server. Mutex melindungi tampilan terminal agar tidak rusak akibat dua thread mencetak bersamaan.

Permainan berakhir ketika semua kapal milik salah satu pemain berhasil ditenggelamkan. Server mengirimkan pesan kemenangan dan kekalahan ke masing-masing pemain.

Pemenang:
```text
========================================
[HASIL] Semua kapal musuh telah tenggelam!
[HASIL] ANDA MENANG!
========================================
```

Yang kalah:
```text
========================================
[HASIL] Semua kapal Anda telah tenggelam.
[HASIL] ANDA KALAH.
========================================
```

### Penyelesaian
- Code Lengkap (player.c bagian threading):
```c
static pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;
static int game_over = 0;
static int my_turn = 0;

void* listener_thread(void* arg) {
    char buf[MSG_SIZE];
    while (!game_over) {
        memset(buf, 0, MSG_SIZE);
        if (mq_receive(mq_in, buf, MSG_SIZE, NULL) > 0) {
            pthread_mutex_lock(&console_mutex);

            if (strncmp(buf, "WAIT", 4) == 0) {
                printf("\n[INFO] %s\n", buf + 5);
                my_turn = 0;
            }
            else if (strncmp(buf, "YOUR_TURN", 9) == 0) {
                my_turn = 1;
                printf("\n========================================\n");
                printf("[GILIRAN ANDA]\n\n");
                // ... render papan lawan dan papan sendiri ...
                printf("Target: ");
                fflush(stdout);
            }
            else if (strncmp(buf, "FIRE_RES", 8) == 0) {
                printf("\n[HASIL TEMBAKAN]\n");
                printf("%s\n", buf + 9);
                printf("========================================\n");
                my_turn = 0;
            }
            else if (strncmp(buf, "OPP_FIRE", 8) == 0) {
                printf("\n[INFO] Lawan menembak %s\n", buf + 9);
            }
            else if (strncmp(buf, "YOU_WIN", 7) == 0) {
                printf("\n========================================\n");
                printf("[HASIL] Semua kapal musuh telah tenggelam!\n");
                printf("[HASIL] ANDA MENANG!\n");
                printf("========================================\n");
                game_over = 1;
            }
            else if (strncmp(buf, "YOU_LOSE", 8) == 0) {
                printf("\n========================================\n");
                printf("[HASIL] Semua kapal Anda telah tenggelam.\n");
                printf("[HASIL] ANDA KALAH.\n");
                printf("========================================\n");
                game_over = 1;
            }
            pthread_mutex_unlock(&console_mutex);
        }
    }
    return NULL;
}

// Di main():
pthread_t tid;
pthread_create(&tid, NULL, listener_thread, NULL);
// ... main loop fgets ...
pthread_join(tid, NULL);
mq_close(mq_in);
mq_close(mq_out);
```

- Code Lengkap (server.c bagian build_turn_msg dan cleanup):
```c
static void build_turn_msg(int pid, char *out, size_t osz) {
    Player *p = &players[pid];
    int n = 0;
    n += snprintf(out + n, osz - n, "YOUR_TURN ");
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char cell = p->board[r][c];
            if (p->hit_board[r][c] == CELL_HIT) cell = CELL_HIT;
            else if (p->hit_board[r][c] == CELL_MISS) cell = CELL_MISS;
            out[n++] = cell;
        }
    }
    out[n++] = '|';
    Player *opp = &players[1 - pid];
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            out[n++] = opp->hit_board[r][c];
    out[n++] = '|';
    out[n] = '\0';
}

static void cleanup(void) {
    mq_close(mq_in[0]);  mq_unlink(MQ_P1_TO_SRV);
    mq_close(mq_in[1]);  mq_unlink(MQ_P2_TO_SRV);
    mq_close(mq_out[0]); mq_unlink(MQ_SRV_TO_P1);
    mq_close(mq_out[1]); mq_unlink(MQ_SRV_TO_P2);
    printf("[SERVER] Pembersihan selesai. Sampai jumpa.\n");
}
```

Penjelasan:
```c
#include <pthread.h>
```
Pustaka `pthread.h` menyediakan API untuk multithreading (POSIX Threads). Saat kompilasi harus disertakan flag `-lpthread`. Library ini dibutuhkan agar player bisa menerima pesan dari server secara asinkron sementara tetap menunggu input dari pengguna.

```c
static pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;
```
Mendeklarasikan mutex global yang diinisialisasi secara statis. Mutex ini berfungsi sebagai kunci untuk melindungi akses ke `stdout`. Tanpa mutex, ketika thread listener mencetak pesan (misalnya "[INFO] Lawan menembak 2C") bersamaan dengan thread utama mencetak prompt input, output di terminal bisa berantakan dan tidak terbaca.

```c
pthread_create(&tid, NULL, listener_thread, NULL);
```
Membuat thread baru yang menjalankan fungsi `listener_thread()`. Thread ini berjalan paralel dengan thread utama. Thread utama menangani `fgets()` (blocking I/O keyboard), sementara thread listener menangani `mq_receive()` (blocking I/O antrean pesan). Karena keduanya bersifat blocking, mereka harus berjalan di thread terpisah agar sistem tetap responsif.

```c
pthread_mutex_lock(&console_mutex);
// ... printf, fflush ...
pthread_mutex_unlock(&console_mutex);
```
Sebelum mencetak apapun ke terminal, thread harus mengunci mutex terlebih dahulu (`lock`). Setelah pencetakan selesai, mutex dilepas (`unlock`). Ini memastikan hanya satu thread yang bisa menulis ke `stdout` pada satu waktu, sehingga output papan dan pesan tidak tercampur.

```c
if (strncmp(buf, "YOUR_TURN", 9) == 0) {
    my_turn = 1;
```
Ketika server mengirim pesan diawali `YOUR_TURN`, listener thread mendeteksinya, menetapkan flag `my_turn = 1`, dan mencetak tampilan papan beserta prompt. Thread utama yang sedang menunggu `fgets()` memeriksa flag ini — jika `my_turn == 1` dan ada input, input dikirim sebagai tembakan.

```c
else if (strncmp(buf, "OPP_FIRE", 8) == 0) {
    printf("\n[INFO] Lawan menembak %s\n", buf + 9);
}
```
Ketika server memberitahu bahwa lawan sudah menembak, listener thread langsung menampilkannya di terminal secara real-time. Berkat mutex, pesan ini tidak akan rusak meskipun thread utama sedang menunggu input.

```c
static void build_turn_msg(int pid, char *out, size_t osz) {
```
Fungsi ini membangun satu string besar yang berisi data papan pemain dan papan lawan. Format data adalah: `YOUR_TURN <16 char papan sendiri>|<16 char papan lawan>|`. Player mem-*parse* string ini di sisi klien untuk menampilkan dua papan terpisah. Pada papan sendiri, jika `hit_board` menunjukkan `CELL_HIT` maka ditampilkan `X`, jika `CELL_MISS` maka ditampilkan ` ` (spasi), menandakan di mana lawan sudah pernah menembak.

```c
static void cleanup(void) {
    mq_close(mq_in[0]);  mq_unlink(MQ_P1_TO_SRV);
```
Fungsi pembersihan menutup semua antrean pesan (`mq_close`) dan menghapusnya dari sistem (`mq_unlink`). Ini penting untuk memastikan tidak ada sumber daya IPC yang bocor setelah permainan berakhir. Tanpa pembersihan, antrean lama akan tetap ada di `/dev/mqueue/` dan bisa mengganggu eksekusi berikutnya.

```c
pthread_join(tid, NULL);
mq_close(mq_in);
mq_close(mq_out);
```
Di sisi player, `pthread_join()` memastikan thread utama menunggu listener thread selesai sebelum menutup antrean. Ini mencegah situasi di mana antrean ditutup sementara listener thread masih mencoba membaca dari antrean tersebut.
