# Pembahasan Task 3 _(Simulasi Pertempuran Laut / Naval Battle Simulation)_

## Kompilasi dan Cara Menjalankan

```
gcc game.c -o game
gcc player.c -o player -lpthread
```

Jalankan server terlebih dahulu, lalu jalankan player di dua terminal terpisah:

```
./game
./player   # terminal 2
./player   # terminal 3
```

---

## Desain Antrean Pesan

Program menggunakan **5 message queue**:

| Nama               | Arah              | Fungsi                            |
| ------------------ | ----------------- | --------------------------------- |
| `MQ_JOIN /bs_join` | player ? server   | Player mendaftar (handshake awal) |
| `MQ_P1 /bs_p1`     | player 1 ? server | Perintah game dari Player 1       |
| `MQ_P2 /bs_p2`     | player 2 ? server | Perintah game dari Player 2       |
| `MQ_S1 /bs_s1`     | server ? player 1 | Respons server ke Player 1        |
| `MQ_S2 /bs_s2`     | server ? player 2 | Respons server ke Player 2        |

Selain itu, setiap player membuat 1 **antrean sementara** (`/bs_tmp_<pid>`) khusus untuk menerima ID pemain saat handshake, lalu langsung dihapus.

### Protokol Pesan

| Token                | Arah              | Makna                                    |
| -------------------- | ----------------- | ---------------------------------------- |
| `JOIN <queue>`       | player ? server   | Daftarkan diri, sertakan nama temp queue |
| `START`              | server ? player   | Kedua pemain sudah terhubung             |
| `PLACE <coord>`      | player ? server   | Tempatkan kapal di koordinat             |
| `OK <pesan>`         | server ? player   | Kapal berhasil ditempatkan               |
| `ERR <pesan>`        | server ? player   | Input tidak valid (placement/fire)       |
| `WAIT_OPP`           | server ? player 1 | Tunggu lawan selesai menempatkan         |
| `READY`              | server ? player   | Semua pemain siap, game dimulai          |
| `TURN <own>\|<shot>` | server ? player   | Giliran bermain + data papan             |
| `WAIT <pesan>`       | server ? player   | Giliran lawan sedang bermain             |
| `FIRE <coord>`       | player ? server   | Tembak koordinat                         |
| `RES <hasil>`        | server ? player   | Hasil tembakan pemain ini                |
| `OPP <hasil>`        | server ? player   | Informasi tembakan lawan                 |
| `WIN`                | server ? player   | Pemain menang                            |
| `LOSE`               | server ? player   | Pemain kalah                             |

---

## a. Persiapan Koneksi dan Antrean Pesan _(Connection Setup and Message Queues)_

### Soal

Buatlah program `game.c` yang berperan sebagai **Game Master** dan program `player.c` sebagai klien pemain interaktif. Server harus membuat dan mengelola **message queue** untuk komunikasi. Dibutuhkan 1 queue publik untuk menerima koneksi pemain baru dan **4 queue privat** untuk komunikasi dua arah saat gameplay.

### Penyelesaian

- Code Lengkap (game.c bagian koneksi):

```
#define MQ_JOIN "/bs_join"
#define MQ_P1   "/bs_p1"
#define MQ_P2   "/bs_p2"
#define MQ_S1   "/bs_s1"
#define MQ_S2   "/bs_s2"
#define MSGSZ   256

static mqd_t mq_in[2], mq_out[2];

int main(void) {
    struct mq_attr attr = {0, 10, MSGSZ, 0};

    mq_unlink(MQ_JOIN); mq_unlink(MQ_P1); mq_unlink(MQ_P2);
    mq_unlink(MQ_S1);   mq_unlink(MQ_S2);

    mq_in[0]  = mq_open(MQ_P1,   O_CREAT | O_RDONLY, 0666, &attr);
    mq_in[1]  = mq_open(MQ_P2,   O_CREAT | O_RDONLY, 0666, &attr);
    mq_out[0] = mq_open(MQ_S1,   O_CREAT | O_WRONLY, 0666, &attr);
    mq_out[1] = mq_open(MQ_S2,   O_CREAT | O_WRONLY, 0666, &attr);
    mqd_t mq_join = mq_open(MQ_JOIN, O_CREAT | O_RDONLY, 0666, &attr);

    for (int i = 0; i < 2; i++) {
        char buf[MSGSZ];
        printf("[SERVER] Menunggu Pemain %d...
", i + 1);
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
            printf("[SERVER] Pemain %d terhubung.
", i + 1);
        } else {
            i--;
        }
    }

    mq_close(mq_join);
    mq_unlink(MQ_JOIN);
    printf("[SERVER] Kedua pemain terhubung. Permainan dimulai!
");
    send_to(0, "START");
    send_to(1, "START");
}
```

- Code Lengkap (player.c bagian koneksi):

```
#define MQ_JOIN "/bs_join"
#define MQ_P1   "/bs_p1"
#define MQ_P2   "/bs_p2"
#define MQ_S1   "/bs_s1"
#define MQ_S2   "/bs_s2"
#define MSGSZ   256

static mqd_t mq_out, mq_in;
static int   player_id;

int main(void) {
    pid_t pid = getpid();
    char  tmp_q[128];
    snprintf(tmp_q, sizeof(tmp_q), "/bs_tmp_%d", (int)pid);

    struct mq_attr attr = {0, 10, MSGSZ, 0};
    mqd_t mq_tmp  = mq_open(tmp_q,  O_CREAT | O_RDONLY, 0666, &attr);
    mqd_t mq_join = mq_open(MQ_JOIN, O_WRONLY);

    char join_msg[MSGSZ];
    snprintf(join_msg, MSGSZ, "JOIN %s", tmp_q);
    mq_send(mq_join, join_msg, strlen(join_msg) + 1, 0);
    mq_close(mq_join);

    char buf[MSGSZ * 2];
    memset(buf, 0, sizeof(buf));
    mq_receive(mq_tmp, buf, sizeof(buf), NULL);
    player_id = atoi(buf);
    mq_close(mq_tmp); mq_unlink(tmp_q);

    printf("[PEMAIN %d] Berhasil terhubung!
", player_id);

    mq_out = mq_open(player_id == 1 ? MQ_P1 : MQ_P2, O_WRONLY);
    mq_in  = mq_open(player_id == 1 ? MQ_S1 : MQ_S2, O_RDONLY);
}
```

Penjelasan:

```
struct mq_attr attr = {0, 10, MSGSZ, 0};
```

Cara singkat menginisialisasi atribut antrean: `mq_flags=0`, `mq_maxmsg=10`, `mq_msgsize=MSGSZ`, `mq_curmsgs=0`. Antrean bisa menampung maksimal 10 pesan, masing-masing berukuran 256 byte.

```
mq_unlink(MQ_JOIN); mq_unlink(MQ_P1); ...
```

Menghapus sisa antrean dari eksekusi sebelumnya. Resource message queue bisa tetap ada setelah program berhenti sampai dihapus secara eksplisit.

```
mq_in[0] = mq_open(MQ_P1, O_CREAT | O_RDONLY, 0666, &attr);
```

Server membuka `MQ_P1` sebagai read-only (hanya menerima dari Player 1). Array `mq_in[2]` dan `mq_out[2]` mempermudah pengaksesan queue berdasarkan indeks pemain (0 atau 1).

```
char tmp_q[128];
snprintf(tmp_q, sizeof(tmp_q), "/bs_tmp_%d", (int)pid);
```

Setiap player membuat antrean sementara unik menggunakan PID prosesnya. Player mengirim nama antrean ini ke server lewat `Q_JOIN`, server membalas dengan ID pemain (`"1"` atau `"2"`) ke antrean sementara tersebut, lalu antrean dihapus.

```
mq_out = mq_open(player_id == 1 ? MQ_P1 : MQ_P2, O_WRONLY);
mq_in  = mq_open(player_id == 1 ? MQ_S1 : MQ_S2, O_RDONLY);
```

Setelah mendapat ID, player membuka queue privat yang sesuai untuk komunikasi selanjutnya.

---

## b. Penempatan Armada _(Fleet Placement)_

### Soal

Setelah kedua pemain terhubung, masing-masing pemain menempatkan 2 kapalnya secara bergantian (Player 1 selesai dahulu, lalu Player 2). Koordinat harus valid dan tidak menimpa kapal yang sudah ada.

### Penyelesaian

- Code Lengkap (game.c bagian penempatan):

```
typedef struct {
    char own[4][4];
    char shot[4][4];
    int  ships;
} Player;

static Player player[2];

static void setup(int p) {
    char buf[MSGSZ];
    printf("[SERVER] Pemain %d sedang menempatkan armada...
", p + 1);
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

// Di main():
setup(0);
send_to(0, "WAIT_OPP");
setup(1);
send_to(0, "READY");
send_to(1, "READY");
```

- Code Lengkap (player.c bagian penempatan):

```
for (int i = 1; i <= 2; i++) {
    while (1) {
        printf("Tempatkan Kapal %d:
> ", i);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL) continue;
        strip(input);

        char cmd[QSZ];
        snprintf(cmd, QSZ, "PLACE %s", input);
        mq_send(q_send, cmd, strlen(cmd) + 1, 0);

        memset(buf, 0, sizeof(buf));
        mq_receive(q_recv, buf, sizeof(buf), NULL);

        if (strncmp(buf, "ERR ", 4) == 0) {
            printf("%s
", buf + 4);
        } else if (strncmp(buf, "OK ", 3) == 0) {
            printf("%s
", buf + 3);
            break;
        }
    }
}
```

Penjelasan:

```
typedef struct {
    char own[4][4];
    char shot[4][4];
    int  ships;
} Player;
```

Struct `Player` disederhanakan menjadi dua papan saja. `own[4][4]` menyimpan papan pemain sendiri dengan satu nilai per sel: `'.'` kosong belum ditembak, `'S'` kapal, `'X'` kapal terkena tembakan lawan, `' '` kosong ditembak lawan (meleset). `shot[4][4]` menyimpan semua tembakan yang sudah dilakukan pemain ini ke lawan: `'.'` belum tembak, `'X'` kena, `' '` meleset.

```
int r = buf[6] - '0';
int c = col_idx(buf[7]);
```

Koordinat dikirim dalam format `"PLACE 2C"`. `buf[6]` adalah digit baris (`'0'`-`'3'`), `buf[7]` adalah huruf kolom (`'A'`-`'D'` atau `'a'`-`'d'`).

```
player[p].own[row][col] = 'S';
player[p].ships++;
```

Menempatkan kapal langsung di papan `own`. Counter `ships` digunakan server untuk mendeteksi kapan semua kapal tenggelam.

```
setup(0);
send_to(0, "WAIT_OPP");
setup(1);
```

Player 1 selesai menempatkan kapal ? server kirim `WAIT_OPP` ? Player 1 tahu harus menunggu ? server proses Player 2. Setelah keduanya selesai, server kirim `READY` ke kedua pemain.

---

## c. Giliran Bermain dan Penembakan _(Turn System and Firing)_

### Soal

Server menentukan pemain pertama secara acak. Setiap giliran, pemain melihat papan lawan (atas) dan papan sendiri (bawah), lalu mengetik koordinat target. Pemain yang sudah pernah menembak petak yang sama, hasilnya selalu `MELESET`.

### Penyelesaian

- Code Lengkap (game.c bagian penembakan):

```
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
            send_to(shooter, "ERR Format tidak valid."); continue;
        }
        int row = buf[5] - '0';
        int col = col_idx(buf[6]);
        if (row < 0 || row > 3 || col < 0) {
            send_to(shooter, "ERR Koordinat di luar batas!"); continue;
        }

        char coord[3] = {buf[5], buf[6], '\0'};
        int hit = 0;

        if (player[shooter].shot[row][col] != '.') {
            hit = 0;
        } else if (player[target].own[row][col] == 'S') {
            hit = 1;
            player[target].own[row][col]   = 'X';
            player[shooter].shot[row][col] = 'X';
            player[target].ships--;
        } else {
            if (player[target].own[row][col] == '.') player[target].own[row][col] = ' ';
            player[shooter].shot[row][col] = ' ';
        }

        printf("[GILIRAN] Pemain %d menembak %s: %s
", shooter+1, coord, hit?"KENA":"MELESET");
        if (hit && player[target].ships == 0)
            printf("[TENGGELAM] Pemain %d menenggelamkan Kapal Pemain %d!
", shooter+1, target+1);

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
    printf("[SERVER] Dipilih secara acak: Pemain %d bermain duluan.
", current + 1);

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
            printf("[SERVER] Pemain %d menang!
", current + 1);
            break;
        }
        current = other;
    }
}
```

- Code Lengkap (player.c bagian menembak):

```
while (!game_over) {
    if (fgets(input, sizeof(input), stdin) == NULL) {
        usleep(100000); continue;
    }
    strip(input);

    pthread_mutex_lock(&mutex);
    int turn = my_turn;
    pthread_mutex_unlock(&mutex);

    if (!turn) continue;

    if (strlen(input) < 2) {
        pthread_mutex_lock(&mutex);
        printf("Format tidak valid! Contoh: 2C
Target: ");
        fflush(stdout);
        pthread_mutex_unlock(&mutex);
        continue;
    }

    pthread_mutex_lock(&mutex);
    my_turn = 0;
    pthread_mutex_unlock(&mutex);

    char cmd[MSGSZ];
    snprintf(cmd, MSGSZ, "FIRE %s", input);
    mq_send(mq_out, cmd, strlen(cmd) + 1, 0);
}
```

Penjelasan:

```
static void build_turn(int p, char *out) {
    int n = sprintf(out, "TURN ");
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[n++] = pl[p].own[r][c];
    out[n++] = '|';
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[n++] = pl[p].shot[r][c];
    out[n] = '\0';
}
```

Membangun pesan giliran: `"TURN <16 char own><|><16 char shot>"`. `own` sudah encoding seluruh state papan sendiri langsung (S/X/space/dot), `shot` encoding apa yang sudah ditembakkan ke lawan. Player hanya perlu memisahkan pada karakter `|`.

```
if (pl[s].shot[r][c] != '.') {
    hit = 0;
}
```

Deteksi tembakan ulang menggunakan `shot` milik penembak, bukan papan lawan. Jika nilai bukan `'.'` (sudah pernah ditembak), hasilnya selalu miss tanpa mengubah state papan apapun.

```
pl[t].own[r][c]  = 'X';
pl[s].shot[r][c] = 'X';
pl[t].ships--;
```

Satu kali kena: papan lawan (`own`) diubah dari `'S'` ke `'X'`, papan `shot` penembak dicatat `'X'`, dan counter kapal lawan dikurangi.

```
snprintf(opp[t], QSZ, "OPP %s: ...", coord, ...);
has_opp[t] = 1;
```

Pesan notifikasi untuk lawan disimpan dulu di buffer `opp[]`, tidak langsung dikirim. Pesan ini baru dikirim di **awal giliran berikutnya** lawan (`has_opp` diperiksa di `run_game()`). Ini memastikan urutan tampilan di terminal lawan tetap rapi.

```
myturn = 0;
snprintf(cmd, QSZ, "FIRE %s", input);
mq_send(q_send, cmd, strlen(cmd) + 1, 0);
```

Flag `myturn` di-set `0` dulu sebelum mengirim perintah. Jika server membalas `ERR`, listener thread akan mengembalikan `myturn = 1` sehingga player bisa mencoba lagi.

---

## d. Pengelolaan Thread, Akhir Permainan, dan Pembersihan _(Thread Management, End of Game, and Cleanup)_

### Soal

`player.c` harus menggunakan **thread** dan **mutex**. Thread utama menangani input keyboard, thread `listener` menangani semua pesan masuk dari server secara asinkron. Mutex melindungi `stdout` dan variabel bersama.

### Penyelesaian

- Code Lengkap (player.c bagian threading):

```
static pthread_mutex_t mutex     = PTHREAD_MUTEX_INITIALIZER;
static volatile int    game_over = 0;
static volatile int    my_turn   = 0;

static void show_board(const char *board, int is_enemy) {
    printf("  A B C D
");
    for (int r = 0; r < 4; r++) {
        printf("%d|", r);
        for (int c = 0; c < 4; c++) {
            char ch = board[r * 4 + c];
            printf("%c|", ch == '.' ? (is_enemy ? '?' : ' ') : ch);
        }
        printf("
");
    }
}

static void *listener(void *arg) {
    (void)arg;
    char buf[MSGSZ * 2];
    while (!game_over) {
        memset(buf, 0, sizeof(buf));
        if (mq_receive(mq_in, buf, sizeof(buf), NULL) < 0) {
            if (game_over) break;
            continue;
        }
        pthread_mutex_lock(&mutex);

        if (strncmp(buf, "TURN ", 5) == 0) {
            my_turn = 1;
            char own[17]={0}, shot[17]={0};
            char *sep = strchr(buf + 5, '|');
            if (sep) {
                int n = sep - (buf+5); if (n > 16) n = 16;
                memcpy(own, buf+5, n);
                memcpy(shot, sep+1, 16);
            }
            printf("
========================================
");
            printf("[GILIRAN ANDA]

");
            printf("    Papan Lawan
"); show_board(shot, 1);
            printf("
    Papan Anda
"); show_board(own, 0);
            printf("
Target: "); fflush(stdout);
        }
        else if (strncmp(buf, "RES ", 4) == 0) {
            printf("
[HASIL TEMBAKAN]
%s
========================================
", buf+4);
            fflush(stdout); my_turn = 0;
        }
        else if (strncmp(buf, "ERR ", 4) == 0) {
            printf("
%s
Target: ", buf+4); fflush(stdout); my_turn = 1;
        }
        else if (strncmp(buf, "OPP ", 4) == 0) {
            printf("
[INFO] Lawan menembak %s
", buf+4); fflush(stdout);
        }
        else if (strcmp(buf, "WIN") == 0) {
            printf("
========================================
");
            printf("[HASIL] Semua kapal musuh telah tenggelam!
[HASIL] ANDA MENANG!
");
            printf("========================================
"); fflush(stdout);
            game_over = 1; pthread_mutex_unlock(&mutex); break;
        }
        else if (strcmp(buf, "LOSE") == 0) {
            printf("
========================================
");
            printf("[HASIL] Semua kapal Anda telah tenggelam.
[HASIL] ANDA KALAH.
");
            printf("========================================
"); fflush(stdout);
            game_over = 1; pthread_mutex_unlock(&mutex); break;
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Di main():
pthread_t tid;
pthread_create(&tid, NULL, listener, NULL);
// ... main loop ...
pthread_join(tid, NULL);
mq_close(mq_in);
mq_close(mq_out);
```

- Code Lengkap (game.c bagian cleanup):

```
static void cleanup(void) {
    mq_close(qin[0]);  mq_unlink(Q_P1);
    mq_close(qin[1]);  mq_unlink(Q_P2);
    mq_close(qout[0]); mq_unlink(Q_S1);
    mq_close(qout[1]); mq_unlink(Q_S2);
    printf("[SERVER] Pembersihan selesai. Sampai jumpa.
");
}
```

Penjelasan:

```
static volatile int game_over = 0;
static volatile int my_turn   = 0;
```

`volatile` memastikan compiler tidak meng-cache nilai di register. Karena dua thread mengakses variabel ini bersamaan, setiap akses harus selalu membaca nilai terbaru dari memori.

```
static void show_board(const char *board, int is_enemy) {
    char ch = board[r * 4 + c];
    printf("%c|", ch == '.' ? (is_enemy ? '?' : ' ') : ch);
}
```

Satu fungsi untuk menampilkan kedua papan. Logika sederhana: jika sel `'.'` (belum ditembak), tampilkan `'?'` untuk papan lawan atau `' '` untuk papan sendiri. Karakter lain (`S`, `X`, ` `) tampil apa adanya.

```
else if (strncmp(buf, "TURN ", 5) == 0) {
    char *sep = strchr(buf + 5, '|');
    int n = sep - (buf + 5); if (n > 16) n = 16;
    memcpy(own, buf + 5, n);
    memcpy(shot, sep + 1, 16);
}
```

Parsing pesan `TURN` menjadi dua papan 16 karakter. Pemisah `|` membatasi `own` (papan sendiri) dan `shot` (tembakan ke lawan). `own` dikirim ke `show_board(..., 0)` dan `shot` ke `show_board(..., 1)`.

```
else if (strncmp(buf, "ERR ", 4) == 0) {
    printf("
%s
Target: ", buf+4); fflush(stdout);
    my_turn = 1;
}
```

Jika server menolak tembakan, listener menampilkan pesan error, kembali mencetak `Target:`, dan mengembalikan `myturn = 1` agar main thread bisa mengirim ulang.

```
game_over = 1;
pthread_mutex_unlock(&mutex);
break;
```

Saat game berakhir (`WIN`/`LOSE`), listener set `done = 1`, lepas mutex, lalu keluar dari loop dengan `break`. Ini menghindari thread terblokir pada `mq_receive()` berikutnya. Main thread membaca `done` dan juga keluar, lalu `pthread_join()` menunggu listener selesai sebelum menutup queue.

```
static void cleanup(void) {
    mq_close(qin[0]); mq_unlink(Q_P1);
    ...
}
```

`mq_close()` menutup file descriptor queue. `mq_unlink()` menghapus queue dari sistem (`/dev/mqueue/`). Keduanya diperlukan agar tidak ada kebocoran IPC setelah game selesai.


