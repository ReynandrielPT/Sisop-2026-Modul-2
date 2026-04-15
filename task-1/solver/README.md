# Pembahasan Task 1 _(Tim Biru / Blue Team)_

## Kompilasi dan Cara Menjalankan

```ngcc extract.c  -o extract
gcc classify.c -o classify
gcc watcher.c  -o watcher
gcc report.c   -o report
```

```n./extract          # ekstrak evidence.zip → logs_dump/, hapus .zip
./classify         # kelompokkan log ke subdir per jenis serangan
./watcher          # jalankan daemon (berjalan di background)
./report           # buat laporan + arsip backup
kill $(cat watcher.pid)  # hentikan daemon
```

---

## a. Initial Extraction _(extract.c)_
### Soal
Buat `extract.c` yang mengekstrak `evidence.zip` ke `logs_dump/` lalu menghapus `evidence.zip`. Proses harus sekuensial. Tidak boleh menggunakan `system()` atau `popen()`.

### Penyelesaian
```n#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

int main(void) {
    mkdir("logs_dump", 0755);

    pid_t pid = fork();
    if (pid == 0) {
        execlp("unzip", "unzip", "-o", "evidence.zip", "-d", "logs_dump", NULL);
        perror("execlp unzip");
        exit(1);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        remove("evidence.zip");
        printf("[Info] evidence.zip berhasil dihapus.\n");
    } else {
        fprintf(stderr, "[Error] Ekstraksi gagal.\n");
        return 1;
    }
    return 0;
}
```

Penjelasan:
```npid_t pid = fork();
if (pid == 0) {
    execlp("unzip", "unzip", "-o", "evidence.zip", "-d", "logs_dump", NULL);
    exit(1);
}
waitpid(pid, &status, 0);
```
Karena `system()` dan `popen()` dilarang, kita gunakan `fork()` + `execlp()`. Child menjalankan `unzip`, parent memblokir di `waitpid()` hingga child selesai — ini memastikan proses berjalan **sekuensial**: ekstraksi dulu, baru hapus zip.

```nif (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    remove("evidence.zip");
```
`remove()` hanya dipanggil jika `unzip` exit code 0 (sukses). Ini mencegah file asli ikut terhapus ketika ekstraksi gagal.

---

## b. Logs Classification _(classify.c)_
### Soal
Buat `classify.c` yang mengelompokkan file log di `logs_dump/` ke subfolder berdasarkan jenis serangan (prefix sebelum `_` pertama). Gunakan multiprocess — satu child per jenis serangan unik.

### Penyelesaian
```n#define MAX_TYPES 64
#define LOGS_DIR  "logs_dump"

static char types[MAX_TYPES][128];
static int  type_count = 0;

int main(void) {
    DIR *dir = opendir(LOGS_DIR);
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;

        char *under = strchr(entry->d_name, '_');
        if (!under) continue;

        int tlen = under - entry->d_name;
        char type[128];
        strncpy(type, entry->d_name, tlen);
        type[tlen] = '\0';
        add_type(type);
    }
    closedir(dir);

    pid_t pids[MAX_TYPES];
    for (int i = 0; i < type_count; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            move_files(types[i]);
            printf("[Child] Selesai mengelompokkan: %s\n", types[i]);
            exit(0);
        }
        pids[i] = pid;
    }

    for (int i = 0; i < type_count; i++)
        waitpid(pids[i], NULL, 0);

    return 0;
}

static void move_files(const char *type) {
    char dest[256];
    snprintf(dest, sizeof(dest), "%s/%s", LOGS_DIR, type);
    mkdir(dest, 0755);

    DIR *dir = opendir(LOGS_DIR);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;

        char *under = strchr(entry->d_name, '_');
        int tlen = under - entry->d_name;
        char found[128];
        strncpy(found, entry->d_name, tlen);
        found[tlen] = '\0';

        if (strcmp(found, type) != 0) continue;

        char src[512], dst[512];
        snprintf(src, sizeof(src), "%s/%s", LOGS_DIR, entry->d_name);
        snprintf(dst, sizeof(dst), "%s/%s/%s", LOGS_DIR, type, entry->d_name);
        rename(src, dst);
    }
    closedir(dir);
}
```

Penjelasan:
```nchar *under = strchr(entry->d_name, '_');
int tlen = under - entry->d_name;
char type[128];
strncpy(type, entry->d_name, tlen);
type[tlen] = '\0';
```
Nama file berformat `Jenis_IP_Timestamp.log`. `strchr(name, '_')` menemukan `_` pertama, lalu kita ambil substring sebelumnya sebagai jenis serangan.

```nfor (int i = 0; i < type_count; i++) {
    pid_t pid = fork();
    if (pid == 0) {
        move_files(types[i]);
        exit(0);
    }
    pids[i] = pid;
}
for (int i = 0; i < type_count; i++)
    waitpid(pids[i], NULL, 0);
```
Satu child per jenis serangan unik dibuat dalam loop, PID disimpan di array. Setelah semua di-fork, parent menunggu semua child selesai satu per satu dengan `waitpid`. Ini parallelism yang aman karena setiap child mengerjakan subset file yang tidak tumpang tindih.

```nrename(src, dst);
```
`rename()` memindahkan file antar direktori dalam filesystem yang sama secara atomik tanpa menyalin byte-per-byte.

---

## c. The Watcher _(watcher.c)_
### Soal
Buat daemon `watcher.c` yang memantau folder `honeypot/` setiap 1 detik. Jika menemukan file `.exe` atau `.pcap`, pindahkan ke `quarantine/` dan catat di `security.log`. Simpan PID ke `watcher.pid`. Handle `SIGTERM` dengan menutup log. Tidak boleh pakai `inotify`/`fanotify`.

### Penyelesaian
```nint main(void) {
    pid_t pid = fork();
    if (pid > 0) exit(0);

    setsid();

    pid_t pid2 = fork();
    if (pid2 > 0) exit(0);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);

    FILE *pidf = fopen(PID_FILE, "w");
    fprintf(pidf, "%d\n", getpid());
    fclose(pidf);

    log_fp = fopen(LOG_FILE, "a");
    signal(SIGTERM, on_sigterm);

    mkdir(HONEYPOT,   0755);
    mkdir(QUARANTINE, 0755);

    while (1) {
        DIR *dir = opendir(HONEYPOT);
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type != DT_REG) continue;
            if (!bad_ext(entry->d_name)) continue;

            log_write("Peringatan! Menemukan file mencurigakan: ...");

            char src[512], dst[512];
            snprintf(src, sizeof(src), "%s/%s", HONEYPOT, entry->d_name);
            snprintf(dst, sizeof(dst), "%s/%s", QUARANTINE, entry->d_name);
            rename(src, dst);

            log_write("Berhasil mengkarantina file: ...");
        }
        closedir(dir);
        sleep(1);
    }
}

static void on_sigterm(int sig) {
    if (log_fp) fclose(log_fp);
    remove(PID_FILE);
    exit(0);
}

static void log_write(const char *msg) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log_fp, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n", ...);
    fflush(log_fp);
}
```

Penjelasan:
```npid_t pid = fork();
if (pid > 0) exit(0);
setsid();
pid_t pid2 = fork();
if (pid2 > 0) exit(0);
```
**Double-fork** adalah cara standar membuat daemon di Unix. Fork pertama memastikan process bukan process leader group (dibutuhkan `setsid()`). `setsid()` membuat session baru (daemon terlepas dari terminal). Fork kedua memastikan daemon bukan session leader, sehingga tidak bisa mendapat controlling terminal secara tidak sengaja.

```numask(0);
chdir("/");
close(STDIN_FILENO); close(STDOUT_FILENO); close(STDERR_FILENO);
open("/dev/null", O_RDONLY); open("/dev/null", O_WRONLY); open("/dev/null", O_WRONLY);
```
`umask(0)` agar file dibuat dengan permission persis yang diminta. `chdir("/")` agar daemon tidak mengunci direktori tertentu. Menutup fd 0/1/2 lalu membuka `/dev/null` menggantikan stdin/stdout/stderr — daemon tidak boleh menulis ke terminal.

```nsignal(SIGTERM, on_sigterm);
```
Handler `on_sigterm` menutup `log_fp` (flush + close) dan menghapus `watcher.pid` sebelum exit. Ini mencegah file log ter-truncate atau data hilang saat daemon dihentikan.

```nwhile (1) {
    DIR *dir = opendir(HONEYPOT);
    // scan...
    closedir(dir);
    sleep(1);
}
```
Polling manual setiap 1 detik menggantikan `inotify`/`fanotify` yang dilarang. `opendir`/`closedir` setiap iterasi memastikan daftar file selalu fresh.

---

## d. Post Incident Archiving & Reporting _(report.c)_
### Soal
Buat `report.c` yang membaca `logs_dump/`, menghitung file per kategori dan frekuensi IP, menulis `threat_report.txt` (diurutkan terbanyak), lalu fork child untuk mengarsip ke `backup_evidence.zip`. Tidak boleh pakai `system()`/`popen()`.

### Penyelesaian
```ntypedef struct { char name[256]; int count; } Entry;

static Entry cats[64];
static int   cat_count = 0;
static Entry ips[1024];
static int   ip_count  = 0;

int main(void) {
    DIR *dir = opendir(LOGS_DIR);
    struct dirent *cat_entry;

    while ((cat_entry = readdir(dir)) != NULL) {
        if (cat_entry->d_type != DT_DIR) continue;
        if (cat_entry->d_name[0] == '.') continue;

        char cat_path[512];
        snprintf(cat_path, sizeof(cat_path), "%s/%s", LOGS_DIR, cat_entry->d_name);
        DIR *sub = opendir(cat_path);

        struct dirent *file;
        while ((file = readdir(sub)) != NULL) {
            if (file->d_type != DT_REG) continue;

            add_entry(cats, &cat_count, 64, cat_entry->d_name);

            char *p1 = strchr(file->d_name, '_');
            char *p2 = strchr(p1 + 1, '_');
            int ip_len = p2 - (p1 + 1);
            char ip[256];
            strncpy(ip, p1 + 1, ip_len);
            ip[ip_len] = '\0';
            add_entry(ips, &ip_count, 1024, ip);
        }
        closedir(sub);
    }
    closedir(dir);

    qsort(cats, cat_count, sizeof(Entry), cmp_desc);
    qsort(ips,  ip_count,  sizeof(Entry), cmp_desc);

    FILE *fp = fopen(REPORT_FILE, "w");
    fprintf(fp, "Laporan Klasifikasi Serangan:\n");
    for (int i = 0; i < cat_count; i++)
        fprintf(fp, "%d. %s muncul sebanyak %d kali.\n", i+1, cats[i].name, cats[i].count);

    fprintf(fp, "\nTop 5 IP Mencurigakan:\n");
    int top = ip_count < 5 ? ip_count : 5;
    for (int i = 0; i < top; i++)
        fprintf(fp, "%d. %s muncul sebanyak %d kali.\n", i+1, ips[i].name, ips[i].count);
    fclose(fp);

    pid_t pid = fork();
    if (pid == 0) {
        execlp("zip", "zip", "-r", ARCHIVE, LOGS_DIR, REPORT_FILE, NULL);
        exit(1);
    }
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        printf("[Info] Laporan dan arsip berhasil dibuat\n");

    return 0;
}
```

Penjelasan:
```nchar *p1 = strchr(file->d_name, '_');
char *p2 = strchr(p1 + 1, '_');
int ip_len = p2 - (p1 + 1);
char ip[256];
strncpy(ip, p1 + 1, ip_len);
```
Nama file `DDoS_192.168.1.10_20240601_120000.log`: `p1` menunjuk `_` setelah jenis, `p2` menunjuk `_` setelah IP. Substring `[p1+1, p2)` adalah IP address.

```nqsort(cats, cat_count, sizeof(Entry), cmp_desc);

static int cmp_desc(const void *a, const void *b) {
    return ((Entry *)b)->count - ((Entry *)a)->count;
}
```
Pengurutan descending menggunakan fungsi komparator yang mengembalikan nilai positif jika `b > a`, menyebabkan `qsort` menempatkan yang lebih besar di depan.

```npid_t pid = fork();
if (pid == 0) {
    execlp("zip", "zip", "-r", ARCHIVE, LOGS_DIR, REPORT_FILE, NULL);
    exit(1);
}
waitpid(pid, &status, 0);
```
`fork` + `execlp("zip")` menggantikan `system("zip ...")` yang dilarang. Child menjalankan `zip -r backup_evidence.zip logs_dump threat_report.txt`. Parent menunggu lalu mencetak pesan sukses.
