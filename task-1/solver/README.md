# Kunci Jawaban — Task 1 Tim Biru
## Praktikum Sistem Operasi: Proses, Multiprocess, dan Daemon

---

## Daftar Isi

1. [Struktur File](#struktur-file)
2. [Cara Compile dan Menjalankan](#cara-compile-dan-menjalankan)
3. [extract.c — Initial Extraction](#extractc--initial-extraction)
4. [classify.c — Logs Classification](#classifyc--logs-classification)
5. [watcher.c — The Watcher Daemon](#watcherc--the-watcher-daemon)
6. [report.c — Post Incident Archiving & Reporting](#reportc--post-incident-archiving--reporting)
7. [Urutan Eksekusi Lengkap](#urutan-eksekusi-lengkap)
8. [Ringkasan Konsep OS](#ringkasan-konsep-os)

---

## Struktur File

```
task-1/
├── extract.c           ← Task a: Ekstraksi otomatis
├── classify.c          ← Task b: Klasifikasi log multiprocess
├── watcher.c           ← Task c: Daemon pengawas honeypot
├── report.c            ← Task d: Laporan dan pengarsipan
├── Makefile            ← Compile semua sekaligus
├── evidence.zip        ← File soal 
└── honeypot_spawner.py ← Tools pengujian watcher 
```

Setelah semua program dijalankan, struktur direktori akan menjadi:

```
task-1/
├── logs_dump/
│   ├── DDoS/
│   ├── Phishing/
│   ├── Malware/
│   ├── Ransomware/
│   ├── Bruteforce/
│   ├── SQLInjection/
│   ├── PortScan/
│   └── XSS/
├── honeypot/
├── quarantine/
├── security.log
├── threat_report.txt
├── backup_evidence.zip
└── watcher.pid
```

---

## Cara Compile dan Menjalankan

### Compile semua sekaligus

```bash
make
```

### Compile satu per satu (jika tidak ada make)

```bash
gcc -Wall -o extract  extract.c
gcc -Wall -o classify classify.c
gcc -Wall -o watcher  watcher.c
gcc -Wall -o report   report.c
```

---

## extract.c — Initial Extraction

### Tujuan

Program ini mengekstrak `evidence.zip` ke folder `logs_dump/`, kemudian menghapus file zip tersebut. Kedua proses harus berjalan **sekuensial** — ekstraksi selesai dulu, baru penghapusan dilakukan.

### Alur Program

```
main()
  │
  ├─► fork() ──► Child 1: execlp("unzip", ...)  ← ekstrak zip
  │
  ├─► waitpid()  ← parent TUNGGU child 1 selesai
  │
  ├─► fork() ──► Child 2: execlp("rm", ...)     ← hapus zip
  │
  └─► waitpid()  ← parent TUNGGU child 2 selesai
```

### Penjelasan Kode

**fork()** menduplikasi proses saat ini. Nilai kembalian:
- `< 0` → fork gagal
- `= 0` → ini adalah child process
- `> 0` → ini adalah parent process, nilainya adalah PID child

```c
pid = fork();
if (pid == 0) {
    execlp("unzip", "unzip", "-o", "evidence.zip", "-d", "logs_dump", NULL);
}
```

Di dalam child process, `execlp()` menggantikan program child dengan program `unzip`. Argumen yang digunakan:
- `"unzip"` (argumen pertama) = nama program yang dicari di PATH
- `"unzip"` (argumen kedua) = `argv[0]`, yaitu nama program itu sendiri
- `"-o"` = overwrite tanpa konfirmasi
- `"evidence.zip"` = file yang diekstrak
- `"-d", "logs_dump"` = direktori tujuan
- `NULL` = penanda akhir argumen (wajib ada)

**waitpid()** membuat parent berhenti sampai child dengan PID tertentu selesai. Ini yang memastikan sekuensialitas — `rm` tidak akan dijalankan sebelum `unzip` benar-benar selesai.

```c
waitpid(pid, &status, 0);
if (WIFEXITED(status) && WEXITSTATUS(status) != 0) { ... }
```

`WIFEXITED` mengecek apakah child keluar secara normal. `WEXITSTATUS` mengambil exit code child — jika bukan 0 berarti ada error.

### Kenapa tidak boleh system() atau popen()?

`system("unzip evidence.zip -d logs_dump")` memang lebih singkat, tapi ia memanggil shell (`/bin/sh -c`), yang artinya rantai prosesnya adalah: `program → shell → unzip`. Ini menyembunyikan konsep fork-exec yang menjadi inti pembelajaran. Dengan `fork()` + `execlp()`, alur pembuatan proses baru terlihat secara eksplisit.

---

## classify.c — Logs Classification

### Tujuan

Program ini membaca semua file log di `logs_dump/`, mengekstrak jenis serangan dari nama file, lalu membuat **satu child process per jenis serangan unik**. Setiap child bertanggung jawab memindahkan semua file milik satu jenis ke dalam subfoldernya masing-masing.

### Format Nama File Log

```
[Jenis Serangan]_[IP Address]_[Timestamp].log
Contoh: DDoS_192.168.1.10_20240601_120000.log
```

Jenis serangan diambil dari bagian sebelum underscore pertama.

### Alur Program

```
main()
  │
  ├─► opendir("logs_dump") — scan semua file
  ├─► kumpulkan jenis serangan unik → attack_types[]
  │
  ├─► for setiap jenis serangan:
  │     └─► fork()
  │           ├─► Parent: lanjut loop (TIDAK menunggu di sini)
  │           └─► Child: mkdir subfolder
  │                       → scan logs_dump
  │                       → pindahkan file milik jenis ini
  │                       → exit(0)
  │
  └─► for i = 0..type_count: wait(NULL)  ← tunggu SEMUA child selesai
```

### Mengapa child dijalankan paralel?

Setelah semua `fork()` dipanggil di dalam loop, parent langsung melanjutkan ke iterasi berikutnya **tanpa menunggu**. Ini membuat semua child berjalan bersamaan. Baru setelah loop selesai, parent memanggil `wait()` sebanyak jumlah child.

Jika `wait()` dipanggil di dalam loop fork (langsung setelah fork), maka parent akan menunggu setiap child selesai sebelum membuat child berikutnya — itu sekuensial, bukan paralel.

### Penjelasan Fungsi Kunci

```c
void extract_attack_type(const char *filename, char *out) {
    strncpy(out, filename, MAX_NAME - 1);
    char *underscore = strchr(out, '_');
    if (underscore) *underscore = '\0';
}
```

Mengambil jenis serangan dari nama file dengan mencari posisi underscore pertama (`strchr`) dan memotong string di sana. `"DDoS_192.168.1.1_..."` menjadi `"DDoS"`.

```c
int already_exists(const char *type) {
    for (int i = 0; i < type_count; i++) {
        if (strcmp(attack_types[i], type) == 0) return 1;
    }
    return 0;
}
```

Memastikan setiap jenis serangan hanya muncul satu kali di array, sehingga jumlah child yang dibuat = jumlah jenis serangan unik.

`rename(src_path, dst_path)` digunakan untuk memindahkan file. Ini lebih efisien dari copy+delete karena hanya mengubah pointer inode di filesystem (selama masih di satu filesystem yang sama).

### Visualisasi Proses Paralel

```
Parent
  │── fork() → Child "DDoS"        ──┐
  │── fork() → Child "Malware"     ──┤  semua berjalan
  │── fork() → Child "Phishing"    ──┤  bersamaan
  │── fork() → Child "Ransomware"  ──┘
  │
  └── wait() × 4   ← tunggu semua selesai
```

---

## watcher.c — The Watcher Daemon

### Tujuan

Program ini berjalan sebagai **daemon** — proses background yang tidak terikat ke terminal. Daemon memantau folder `honeypot/` setiap 1 detik. Jika menemukan file berekstensi `.exe` atau `.pcap`, daemon memindahkannya ke `quarantine/` dan mencatat aktivitas ke `security.log`.

### Apa itu Daemon?

Daemon adalah proses yang berjalan di background, tidak memiliki terminal pengendali, dan tidak terikat ke sesi login pengguna. Contoh daemon di Linux: `sshd`, `nginx`, `cron`. Karakteristik daemon:

- Tidak memiliki terminal (`TTY = ?` di `ps aux`)
- PID-nya disimpan di file `.pid` agar bisa dikontrol
- Dimatikan dengan sinyal, bukan Ctrl+C
- Log ditulis ke file karena tidak ada stdout

### Proses Daemonisasi — Langkah demi Langkah

#### Langkah 1: Fork pertama

```c
pid = fork();
if (pid > 0) exit(EXIT_SUCCESS);   // parent langsung keluar
// child melanjutkan...
```

Parent keluar sehingga shell menganggap program sudah selesai dan prompt dikembalikan ke pengguna. Child terus berjalan di background.

#### Langkah 2: setsid()

```c
if (setsid() < 0) exit(EXIT_FAILURE);
```

`setsid()` membuat **sesi baru**. Child menjadi pemimpin sesi baru yang tidak memiliki terminal pengendali. Ini memutus hubungan proses dari terminal asal sehingga sinyal seperti `SIGHUP` tidak diterima lagi.

#### Langkah 3: Fork kedua

```c
pid = fork();
if (pid > 0) exit(EXIT_SUCCESS);   // pemimpin sesi keluar
// child kedua melanjutkan sebagai daemon sesungguhnya
```

Fork kedua memastikan daemon bukan lagi pemimpin sesi. Ini mencegah daemon mendapat terminal pengendali baru secara tidak sengaja (aturan POSIX: hanya pemimpin sesi yang bisa mendapat terminal).

#### Langkah 4: Tutup dan redirect file descriptor

```c
close(STDIN_FILENO);
close(STDOUT_FILENO);
close(STDERR_FILENO);
int devnull = open("/dev/null", O_RDWR);
dup2(devnull, STDIN_FILENO);
dup2(devnull, STDOUT_FILENO);
dup2(devnull, STDERR_FILENO);
close(devnull);
```

Karena daemon tidak memiliki terminal, stdin/stdout/stderr harus ditutup. Ketiganya diarahkan ke `/dev/null` agar tidak terjadi error jika ada kode yang mencoba menulis ke stdout.

### Visualisasi Daemonisasi

```
Shell
  └─► ./watcher   (proses awal, terhubung ke terminal)
        │
        ├─► fork() #1
        │     ├─► Parent (awal) → exit()  ← shell kembali ke prompt
        │     └─► Child #1
        │           ├─► setsid()   ← sesi baru, lepas dari terminal
        │           │
        │           ├─► fork() #2
        │           │     ├─► Child #1 → exit()
        │           │     └─► Child #2  ←── DAEMON SESUNGGUHNYA
        │           │           ├─► tutup stdin/stdout/stderr
        │           │           ├─► mkdir honeypot/, quarantine/
        │           │           ├─► tulis PID ke watcher.pid
        │           │           ├─► buka security.log
        │           │           └─► loop polling setiap 1 detik
        │           └─► (selesai)
        └─► (selesai)
```

### Signal Handling dengan sigaction()

```c
struct sigaction sa;
sa.sa_handler = handle_sigterm;
sigemptyset(&sa.sa_mask);
sa.sa_flags = 0;
sigaction(SIGTERM, &sa, NULL);
```

`sigaction()` digunakan (bukan `signal()`) karena perilakunya lebih terdefinisi dan portabel di berbagai sistem UNIX. Saat daemon menerima `SIGTERM`, fungsi `handle_sigterm()` dipanggil secara otomatis oleh OS.

```c
void handle_sigterm(int sig) {
    (void)sig;
    write_log("Daemon dihentikan.");
    if (log_fp) fclose(log_fp);   // flush dan tutup file log
    remove(PID_FILE);              // hapus watcher.pid
    exit(EXIT_SUCCESS);
}
```

Menutup file log sebelum exit sangat penting — `fclose()` memastikan semua data di buffer C ditulis ke disk. Tanpa ini, baris terakhir di log bisa hilang karena masih tersimpan di buffer memori.

### Deteksi Ekstensi File

```c
char *dot = strrchr(fname, '.');
if (!dot) continue;
if (strcmp(dot, ".exe") != 0 && strcmp(dot, ".pcap") != 0) continue;
```

`strrchr()` mencari karakter dari kanan (right), sehingga dot **terakhir** yang ditemukan adalah ekstensi file yang benar. Ini penting untuk file dengan nama seperti `update.v2.pcap` — `strchr` dari kiri akan menemukan `.v2` bukan `.pcap`.

### Format security.log

```
[2024-06-01 12:00:00] Daemon watcher dimulai.
[2024-06-01 12:00:03] Peringatan! Menemukan file mencurigakan: backdoor.exe
[2024-06-01 12:00:03] Berhasil mengkarantina file: backdoor.exe
[2024-06-01 12:00:06] Peringatan! Menemukan file mencurigakan: c2_traffic.pcap
[2024-06-01 12:00:06] Berhasil mengkarantina file: c2_traffic.pcap
```

### Cara Menghentikan Daemon

```bash
kill $(cat watcher.pid)
```

Perintah ini membaca PID dari `watcher.pid` lalu mengirim `SIGTERM` ke daemon. Daemon menangkap sinyal ini, menulis log penutup, menutup file, dan keluar dengan bersih.

---

## report.c — Post Incident Archiving & Reporting

### Tujuan

Program ini membaca `logs_dump/` yang sudah terklasifikasi, menghitung jumlah file per kategori serangan dan per IP address, menulis hasilnya ke `threat_report.txt` secara terurut (descending), lalu membuat arsip `backup_evidence.zip` menggunakan child process.

### Alur Program

```
main()
  │
  ├─► opendir("logs_dump") → iterasi tiap subfolder (DDoS, Malware, dst)
  │     └─► tiap subfolder → iterasi tiap file .log
  │           ├─► extract_attack() → increment attacks[]
  │           └─► extract_ip()     → increment ips[]
  │
  ├─► qsort(attacks, ..., cmp_desc)   ← urutkan serangan descending
  ├─► qsort(ips, ..., cmp_desc)       ← urutkan IP descending
  │
  ├─► fopen("threat_report.txt", "w") → tulis laporan
  │
  ├─► fork()
  │     ├─► Parent: waitpid() ← tunggu zip selesai
  │     └─► Child: execlp("zip", "-r", "backup_evidence.zip", ...)
  │
  └─► printf("[Info] Laporan dan arsip berhasil dibuat")
```

### Struktur Data

```c
typedef struct {
    char name[MAX_NAME];   // nama serangan atau IP
    int  count;            // jumlah kemunculan
} Entry;

Entry attacks[MAX_ENTRIES];
Entry ips[MAX_ENTRIES];
```

Fungsi `increment()` mencari nama di array — jika ada, counter ditambah; jika belum ada, ditambahkan sebagai entry baru.

### Ekstraksi IP dari Nama File

```c
void extract_ip(const char *fname, char *out) {
    const char *first  = strchr(fname, '_');   // posisi _ pertama
    first++;                                    // lewati karakter _
    const char *second = strchr(first, '_');   // posisi _ kedua
    size_t len = second - first;               // panjang string IP
    strncpy(out, first, len);
    out[len] = '\0';
}
```

Dari `"DDoS_192.168.1.10_20240601_120000.log"`:
1. `first` → `"192.168.1.10_20240601_120000.log"`
2. `second` → `"_20240601_120000.log"`
3. `len = second - first` = 12 (panjang `"192.168.1.10"`)
4. Hasil: `"192.168.1.10"`

### Pengurutan dengan qsort()

```c
int cmp_desc(const void *a, const void *b) {
    return ((Entry *)b)->count - ((Entry *)a)->count;
}

qsort(attacks, attack_count, sizeof(Entry), cmp_desc);
```

`qsort()` adalah fungsi pengurutan dari stdlib C yang menggunakan fungsi comparator kustom. Comparator `cmp_desc` mengembalikan nilai positif jika `b.count > a.count`, yang memberitahu `qsort` bahwa `b` harus berada sebelum `a` — menghasilkan urutan descending.

### Child Process untuk Zip

```c
pid_t pid = fork();
if (pid == 0) {
    execlp("zip", "zip", "-r", "backup_evidence.zip",
           LOGS_DIR, REPORT_FILE, NULL);
    exit(EXIT_FAILURE);
}
int status;
waitpid(pid, &status, 0);
```

Child menjalankan `zip -r backup_evidence.zip logs_dump/ threat_report.txt`. Flag `-r` berarti rekursif, sehingga seluruh isi subfolder di dalam `logs_dump/` ikut terarsip. Parent menunggu proses zip selesai sebelum mencetak pesan sukses.

### Format threat_report.txt

```
Laporan Klasifikasi Serangan:
1. Phishing muncul sebanyak 2551 kali.
2. Ransomware muncul sebanyak 1710 kali.
3. Malware muncul sebanyak 1314 kali.
4. Bruteforce muncul sebanyak 1036 kali.
5. SQLInjection muncul sebanyak 828 kali.
6. PortScan muncul sebanyak 684 kali.
7. DDoS muncul sebanyak 265 kali.
8. XSS muncul sebanyak 166 kali.

Top 5 IP Mencurigakan:
1. 202.111.175.31 muncul sebanyak 884 kali.
2. 103.43.118.200 muncul sebanyak 876 kali.
3. 176.31.182.86 muncul sebanyak 841 kali.
4. 192.162.240.162 muncul sebanyak 839 kali.
5. 5.79.75.135 muncul sebanyak 804 kali.
```

---

## Urutan Eksekusi Lengkap

```bash
# Pastikan evidence.zip ada
ls evidence.zip

# Compile semua program
make

# Task a: ekstraksi
./extract
# Verifikasi: ls logs_dump/ | head -5 && ls evidence.zip 2>&1

# Task b: klasifikasi paralel
./classify
# Verifikasi: ls logs_dump/ && find logs_dump -name "*.log" | wc -l

# Task c: jalankan daemon (terminal ini)
./watcher
# Verifikasi: cat watcher.pid && ps aux | grep watcher

# Task c: uji dengan spawner (terminal lain)
python3 honeypot_spawner.py --total 15 --interval 2 --seed 42
# Verifikasi: cat security.log && ls quarantine/

# Hentikan daemon setelah pengujian selesai
kill $(cat watcher.pid)

# Task d: laporan dan arsip (jalankan setelah klasifikasi selesai)
./report
# Verifikasi: cat threat_report.txt && ls -lh backup_evidence.zip
```

---

## Ringkasan Konsep OS

| Konsep | Digunakan di | Penjelasan Singkat |
|--------|-------------|-------------------|
| `fork()` | Semua file | Membuat child process baru yang merupakan duplikat parent |
| `execlp()` | extract.c, report.c | Menggantikan image proses dengan program lain |
| `waitpid()` | extract.c, report.c | Parent menunggu child tertentu — sekuensial |
| `wait()` | classify.c | Parent menunggu child mana saja yang selesai — paralel |
| `setsid()` | watcher.c | Membuat sesi baru, memutus dari terminal |
| `sigaction()` | watcher.c | Mendaftarkan signal handler yang portabel |
| `SIGTERM` | watcher.c | Sinyal permintaan berhenti dengan bersih |
| `opendir()` / `readdir()` | classify.c, report.c | Membaca isi direktori |
| `rename()` | classify.c, watcher.c | Memindahkan file secara atomik |
| `qsort()` | report.c | Pengurutan dengan comparator kustom |
| Multiprocess paralel | classify.c | Semua child dibuat dulu, baru `wait()` bersama |
| Multiprocess sekuensial | extract.c | `waitpid()` setelah setiap `fork()` |
| Daemonisasi | watcher.c | `fork()` × 2 + `setsid()` + redirect fd ke `/dev/null` |
| Polling | watcher.c | Loop tak terbatas dengan `sleep(1)` sebagai interval |
