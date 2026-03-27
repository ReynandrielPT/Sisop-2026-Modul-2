# Pembahasan / Solution - Task 3 (Naval Battle Simulation)

---

## a. Persiapan Koneksi dan Antrean Pesan _(Connection Setup and Message Queues)_
### Soal
Buatlah program `server.c` yang berperan sebagai **Game Master** dan program `player.c` sebagai klien pemain interaktif.
Server harus membuat dan mengelola **4 POSIX Message Queue** untuk komunikasi dua arah antara server dan masing-masing pemain: 2 queue untuk menerima pesan dari setiap pemain, dan 2 queue untuk mengirimkan respons ke setiap pemain.
Player dijalankan sederhana tanpa argumen tambahan: `./player`. Program pertama yang terhubung akan ditugaskan sebagai Player 1, dan yang kedua sebagai Player 2.

### Penyelesaian
- Code Lengkap:
```c
// [Bagian SETUP & KONEKSI dari server.c]
#define MQ_BS_JOIN    "/mq_bs_join"
#define MQ_P1_TO_SRV  "/mq_bs_p1_srv"
#define MQ_P2_TO_SRV  "/mq_bs_p2_srv"
#define MQ_SRV_TO_P1  "/mq_bs_srv_p1"
#define MQ_SRV_TO_P2  "/mq_bs_srv_p2"
#define MSG_SIZE      512

int main(void) {
    struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10,
                            .mq_msgsize = MSG_SIZE, .mq_curmsgs = 0 };

    mq_unlink(MQ_BS_JOIN);
    mq_unlink(MQ_P1_TO_SRV); mq_unlink(MQ_P2_TO_SRV);
    mq_unlink(MQ_SRV_TO_P1); mq_unlink(MQ_SRV_TO_P2);

    printf("[SERVER] Battleship Game Master started.\n");

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
    for (int i = 0; i < 2; i++) init_board(&players[i]);

    /* Wait for both players to send CONNECT via the join queue */
    for (int i = 0; i < 2; i++) {
        char buf[MSG_SIZE];
        printf("[SERVER] Waiting for Player %d...\n", i + 1);
        memset(buf, 0, MSG_SIZE);
        mq_receive(mq_join, buf, MSG_SIZE, NULL);
        
        if (strncmp(buf, "CONNECT ", 8) == 0) {
            char temp_q[128];
            sscanf(buf + 8, "%s", temp_q);
            
            /* Reply with assigned ID */
            mqd_t mq_temp = mq_open(temp_q, O_WRONLY);
            if (mq_temp != (mqd_t)-1) {
                char reply[32];
                snprintf(reply, sizeof(reply), "%d", i + 1);
                mq_send(mq_temp, reply, strlen(reply) + 1, 0);
                mq_close(mq_temp);
            }
            
            players[i].connected = 1;
            printf("[SERVER] Player %d connected.\n", i + 1);
        } else {
            i--; /* Retry if invalid message */
        }
    }
    
    mq_close(mq_join);
    mq_unlink(MQ_BS_JOIN);
    
    printf("[SERVER] Both players connected. Game starting!\n");
    srv_send(0, "GAME_START");
    srv_send(1, "GAME_START");
}
```
Penjelasan:
```c
#define MQ_BS_JOIN    "/mq_bs_join"
#define MQ_P1_TO_SRV  "/mq_bs_p1_srv"
...
```
Bagian pendefinisian nama antrean pesan (Message Queues) POSIX. `MQ_BS_JOIN` difungsikan sebagai titik masuk (entry point) publik di mana para pemain (klien) meminta koneksi ketika baru dinyalakan. Sedangkan 4 channel lain dialokasikan khusus untuk Game Master bertukar data dengan Player 1 dan Player 2 secara terisolasi. Parameter `#define MSG_SIZE 512` menentukan batas aman ukuran satu paket data yang masuk atau keluar.

```c
struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10, .mq_msgsize = MSG_SIZE, .mq_curmsgs = 0 };
```
Mendeklarasikan atribut antrean `attr` agar menampung antrean konstan sebanyak maksimum 10 pesan dengan *payload message size* sebesar 512 byte tiap detaknya.

```c
mq_unlink(MQ_BS_JOIN);
...
```
Kode ini bertugas me-*reset* sistem IPC secara manual setiap kali *server* tereksekusi. Terkadang sistem operasi tidak me-nyingkirkan (*unlink*) Message Queue yang menggantung secara *zombie* pada direktori `/dev/mqueue` bila game dihentikan paksa sebelumnya, jadi ini mencegah *memory leak* dan tumpang tindih pesan *cache* terdahulu.

```c
mq_in[0]  = mq_open(MQ_P1_TO_SRV, O_CREAT | O_RDONLY, 0666, &attr);
```
Perintah `mq_open` menginstansiasi masing-masing dari kelima Message Queue di sisi Server. Flag `O_CREAT` memerintahkan agar OS menciptakan file file/antrean sementara baru apabila belum ada, dan memberikan izin akses global *read and write* seutuhnya dengan argumen *permission* `0666`. `mq_in` ditugaskan mengambil R (*Read-Only*) sedangkan `mq_out` menugaskan W (*Write-Only*).

```c
for (int i = 0; i < 2; i++) {
        char buf[MSG_SIZE];
        mq_receive(mq_join, buf, MSG_SIZE, NULL);
```
*Looping* ini merupakan algoritma pendaftaran secara *dynamic join* agar user cukup run `./player` di terminal lain. *Loop* ditahan hingga antrean join (`MQ_BS_JOIN`) menerima setoran string dari klien. 

```c
if (strncmp(buf, "CONNECT ", 8) == 0) {
```
Barisan ini bertugas menyaring segala paket IPC masuk. Skrip hanya memberikan respons identitas apabila paket dimulai dengan kata *CONNECT*. Jika tidak sinkron, maka iterasi mundur (`i--`) memaksa iterasi tersebut dieksekusi ulang hingga Player yang tervalidasi menetas masuk.

```c
mq_open(temp_q, O_WRONLY);
snprintf(reply, sizeof(reply), "%d", i + 1);
mq_send(mq_temp, reply, strlen(reply) + 1, 0);
```
Server mengekstrak rute alamat kanal rahasia klien (*temporary queue* milik _player_), mengirim balik token id pemain seperti parameter angka `1` atau `2` lalu langsung menutup jalur `mq_temp`. Ini meyakinkan pemain mengetahui perannya sebagai Player 1 atau 2 sebelum terjun pada _gameplay_.

```c
mq_close(mq_join);
mq_unlink(MQ_BS_JOIN);
```
Setelah P1 dan P2 tersambung penuh, fungsi *join channel* tak lagi dibutuhkan dalam ingatan program. Karenanya, _Game Master_ secara elegan menutup paksa gerbang IPC `mq_join` agar sesi bermain tidak bisa disabotase klien ketiga.

### Foto Hasil Output
*(Terminal menampilkan "[SERVER] Waiting for Player 1..." disusul koneksi secara real-time antar Player 1 dan Player 2)*

---

## b. Pemilihan dan Penempatan Armada _(Fleet Selection and Placement)_
### Soal
Setelah kedua pemain terhubung, masing-masing pemain diminta memilih komposisi armadanya dan menempatkan kapal-kapalnya secara terpisah dan bersamaan. Pemain mengetikkan jumlah masing-masing kapal. Format masukan adalah jumlah diikuti simbol kapal, lalu penempatannya ditaruh divalidasi lurus dan tanpa benturan.

### Penyelesaian
- Code Lengkap:
```c
// [Bagian SETUP FLEET dari server.c]
static void setup_player(int pid) {
    Player *p = &players[pid];
    char buf[MSG_SIZE];

    /* Expect FLEET message */
    while (1) {
        srv_recv(pid, buf);
        /* Parse "1S 2D" etc. */
        int ns = 0, nd = 0, nc = 0;
        char *tok = strtok(buf, " ");
        int valid = 1;
        while (tok) {
            int qty = 0;
            char type = 0;
            if (sscanf(tok, "%d%c", &qty, &type) == 2) {
                if (type == 'S' || type == 's') ns += qty;
                else if (type == 'D' || type == 'd') nd += qty;
                else if (type == 'C' || type == 'c') nc += qty;
                else { valid = 0; break; }
            } else { valid = 0; break; }
            tok = strtok(NULL, " ");
        }
        if (!valid || (ns + nd + nc) < 1) {
            srv_send(pid, "FLEET_ERR Invalid ship types.");
            continue;
        }
        int total_pts = ns * 2 + nd * 2 + nc * 3;
        if (total_pts > FLEET_MAX_PTS) {
            srv_send(pid, "FLEET_ERR Fleet exceeds 7 point budget.");
            continue;
        }

        char ok_msg[MSG_SIZE];
        snprintf(ok_msg, MSG_SIZE, "FLEET_OK %dS %dD %dC", ns, nd, nc);
        srv_send(pid, ok_msg);
        
        // ... Logika Placement Menyusul ...
```
Penjelasan:
```c
srv_recv(pid, buf);
```
Menggunakan fungsi khusus pembacaan IPC yang menyalin keseluruhan informasi ke dalam matriks `buf`. Aliran berjalan memisahkan data korelasi dari proses P1 maupun P2 berbasis identifikasi _pid_.

```c
char *tok = strtok(buf, " ");
```
Merupakan pemotongan rantai masukan (_string tokenization_) berdasarkan spasi. Modul membolehkan bentuk `1S 2D` dipisah utuh secara struktural ke masing-masing komputasi variabel di memori iterasi _while_ tanpa peduli *prefix* tambahan. 

```c
if (sscanf(tok, "%d%c", &qty, &type) == 2)
```
Sebuah metode parser _scanf_ string tingkat lanjut. Server langsung memetakan format desimal integer pada `%d` sebagai jumlah dan ekstrak alfabet dengan `%c` untuk memilah jenis tipe kapal (`S` / `D` / `C`), mengurangi rumitnya verifikasi algoritma.

```c
int total_pts = ns * 2 + nd * 2 + nc * 3;
if (total_pts > FLEET_MAX_PTS)
```
Sesuai soal, poin maksimum armada hanya dimodali $7$ angka _points_. Setelah program merekap data dari perulangan jenis string, variabel bobot kapal (`2` kali jumlah *sub*, `3` kali *cruiser*) ditotal secara matematik. `FLEET_ERR` mentransfer pelarangan penempatan karena anggaran melebihi budget memicu `continue` untuk loop ulang.

```c
static int place_ship(Player *p, char type, int idx, int r1, int c1, int r2, int c2) {
    int size = (type == SHIP_CRUISER) ? 3 : 2;
    // ...
```
Sisi pengerjaan `server.c` juga membekali validasi posisi kordinat `0A 3A` dari klien. Jika hasil nilai mutlak _row_ dan _column_ yang dibandingkan tidak berhimpit pada _size_ (`span != size`), tidak konstan linear `(r1 != r2 && c1 != c2)` (_diagonal_), apalagi tabrakan dalam *array memory* `p->board[r][c] != CELL_EMPTY`, string IPC kembali menolak klien.

### Foto Hasil Output
*(Console mem-print pesan Fleet confirmed beserta peringatan apabila input yang disusun melebihi target anggaran 7 points)*

---

## c. Gameplay dan Sinkronisasi Giliran _(Gameplay and Turn Synchronization)_
### Soal
Setiap giliran, pemain akan melihat papan lawan dan bebas menembak. Jika menembak kapal tipe *Destroyer* harus *cooldown* 2 turn, dan tipe *Cruiser* 3 turn. Format tembakan pun dikontrol secara valid dan interaktif.

### Penyelesaian
- Code:
```c
// [Bagian LOGIKA PENEMBAKAN GAME LOOP dari player.c]
while (num_coords < target_count) {
    pthread_mutex_lock(&console_mutex);
    printf("Target %d (%d shot(s) remaining): ", num_coords + 1, target_count - num_coords);
    fflush(stdout);
    pthread_mutex_unlock(&console_mutex);
    
    char extra_input[100];
    if (fgets(extra_input, sizeof(extra_input), stdin) != NULL) {
        remove_newline(extra_input);
        char *etok = strtok(extra_input, " ");
        while (etok && num_coords < 10) {
            strncpy(coords[num_coords], etok, 7);
            num_coords++;
            etok = strtok(NULL, " ");
        }
    }
    
    if (num_coords > target_count) {
        pthread_mutex_lock(&console_mutex);
        printf("Jumlah shot terlalu banyak!\nPilih kapal / Select a ship (e.g., S1, D1): ");
        fflush(stdout);
        pthread_mutex_unlock(&console_mutex);
        break;
    }
}
```

Penjelasan:
```c
while (num_coords < target_count) {
```
Potongan algoritma *user-input validator* di sisi klien (*Player*). Selama jumlah tembakan target belum sama dengan beban amunisi total (`target_count`), iterasi dialog penembakan terus ditawarkan ke klien pada stdin, mewujudkan sistem asinkron namun efisien tanpa mencemari sistem _server_.

```c
char *etok = strtok(extra_input, " ");
while (etok && num_coords < 10)
```
Apabila klien memasukkan sekolompok tebakan majemuk dalam spasi seperti `1A 1B 1C`, *loop block tokenizer* tersebut langsung membedah input sebagai nilai entri jamak (kordinat jamak) lalu merekap masing-masing data murni kordinat hingga maksimal ke-10 (*safety constraint*).

```c
if (num_coords > target_count) {
    printf("Jumlah shot terlalu banyak!\n");
```
Sebuah percabangan pengondisian kritikal _error handler_ saat pemain sengaja/tidak iseng menyelipkan 4 pasang tebakan kepada aset *Cruiser* yang semestinya hanya 3 pasang proyektil. Apabila variabel penampung `num_coords` tembus melebihi pagu (*ceiling*) `target_count`, `player.c` secepat kilat melempar laporan teks `Jumlah shot terlalu banyak!` dan membunuh sesi penguraian untuk memaksa prompt meminta tipe kapal diulangi.

```c
// [Dari server.c update status turn & hit board]
for (int si = 0; si < target->num_ships; si++) {
    Ship *ts = &target->ships[si];
    if (ts->sunk) continue;
    for (int ti = 0; ti < ts->size; ti++) {
        if (ts->tiles[ti][0] == r && ts->tiles[ti][1] == c) {
            ts->hits++;
            target->hit_board[r][c] = CELL_HIT;
            // ...
```
Sementara itu dari kubu Game Master, matriks pertempuran diiterasi penuh satu per satu di setiap koordinat kapal musuh `target->ships[si]`. Koordinat yang bertubrukan persis mendarat dan mengekstrak _Cell Value_ menjadi simbol `CELL_HIT`. Data _health pool_ `ts->hits++` akan dikalkulasikan apabila nilai benturannya sama persis menyentuh `ts->size`, yang maka mentransfer status _ship structure_ menjadi _SUNK_ / tenggelam.

### Foto Hasil Output
*(Terlampir foto prompt output "Jumlah shot terlalu banyak!" dan skenario pengguliran bergantian antara terminal Player 1 dan 2 dalam mengeksplor tebakan-tebakan HIT dan MISS)*

---

## d. Multithreading pada Klien _(Client Multithreading)_
### Soal
Program klien pemain haruslah dibuat dengan pustaka *POSIX Threads* dan proteksi mutex supaya UI asinkron bisa tampil meski sistem menunggu prompt `fgets` ketikan *user*.

### Penyelesaian
- Code:
```c
// [Bagian THREADING dari player.c]
pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;

void *listener_thread(void *arg) {
    char buf[MSG_SIZE * 8];
    while (1) {
        memset(buf, 0, sizeof(buf));
        mq_receive(mq_in, buf, sizeof(buf), NULL);
        
        pthread_mutex_lock(&console_mutex);
        
        if (strncmp(buf, "YOUR_TURN", 9) == 0) {
            printf("\n========================================\n");
            printf("[YOUR TURN]\n\n");
            
            /* Parse board and cooldown info */
            char *p = buf + 10;
            // Cetak UI Board...
            my_turn = 1;
            
            // ... Parse cooldown...
            printf("\nPilih kapal / Select a ship (e.g., S1, D1): ");
            fflush(stdout);

        } else if (strncmp(buf, "WAIT", 4) == 0) {
            printf("[INFO] %s\n", buf + 5);
        // ... (banyak percabangan notifikasi lain) ...
        }
        pthread_mutex_unlock(&console_mutex);
    }
    return NULL;
}

// Dalam main()
int main(void) {
    // ... setup join channel ...
    pthread_t tid;
    pthread_create(&tid, NULL, listener_thread, NULL);
    // Loop input stdin dengan fgets()...
}
```

Penjelasan:
```c
pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;
```
Diwajibkan mendeklarasikan Variabel *Mutex* (Mutual Exclusion) global secara makro inisialisasi awal. Secara filosofis, ketika *game* berjalan, utas baca (*listener_thread*) dan eksekusi utama (di *main()*) terus berebut menyuapi baris CLI `stdout`. *Mutex* berfungsi memberi jatah pakem blokade ke salah satu utas.

```c
pthread_create(&tid, NULL, listener_thread, NULL);
```
Siklus `main()` secara langsung membangkitkan anak cabang benang dengan API `pthread_create()`. Begitu dideklarasi, utas utama akan langsung melanjutkan perjalanannya menuju `while(1) { fgets(...) }` di mana ia secara total mendiamkan sisa terminal selama proses blok pengetikan, namun latar belakang anak *thread* terus responsif secara paralel dalam fungsi `listener_thread`.

```c
pthread_mutex_lock(&console_mutex);
...
pthread_mutex_unlock(&console_mutex);
```
Sebagai proteksi _memory leak_ dan malfungsi tumpang tindih _buffer_, utas yang hendak me-render teks respons ke antarmuka terminal seperti pemberitahuan musuh melakukan eksekusi/HIT, memaksa kernel mengunci *(lock)* _console_mutex_. Utas input pengetikan (misal ketika prompt) harus pasrah menanti hingga batas teks _unlock_ ditransfer utuh ke OS CLI demi mencegah rusaknya hierarki display papan permainan kapal akibat sela-menyela _keyboard_.

---
