#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>

#define HONEYPOT_DIR  "honeypot"
#define QUARANTINE_DIR "quarantine"
#define PID_FILE      "watcher.pid"
#define LOG_FILE      "security.log"

// File log dibuat global agar bisa ditutup oleh signal handler
FILE *log_fp = NULL;

// ── Tulis timestamp ke security.log ──────────────────────────────────────
void write_log(const char *message) {
    if (!log_fp) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    fprintf(log_fp, "[%s] %s\n", timestamp, message);
    fflush(log_fp); // pastikan langsung ditulis ke disk
}

// ── Signal handler untuk SIGTERM ─────────────────────────────────────────
// Dipanggil saat: kill $(cat watcher.pid)
void handle_sigterm(int sig) {
    (void)sig;
    write_log("Daemon dihentikan.");

    if (log_fp) fclose(log_fp);

    // Hapus PID file saat keluar
    remove(PID_FILE);
    exit(EXIT_SUCCESS);
}

// ── Proses daemonisasi ────────────────────────────────────────────────────
void daemonize() {
    pid_t pid;

    // Fork pertama: parent keluar, child lanjut
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // parent exit

    // Buat sesi baru — lepas dari terminal
    if (setsid() < 0) exit(EXIT_FAILURE);

    // Fork kedua: pastikan daemon tidak bisa mendapat terminal lagi
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    // Tutup stdin, stdout, stderr
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Arahkan fd 0/1/2 ke /dev/null agar tidak error jika ada yang menulis
    int devnull = open("/dev/null", O_RDWR);
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);
}

int main() {
    // ── Daemonisasi ───────────────────────────────────────────────────────
    daemonize();

    // ── Daftarkan signal handler SIGTERM ──────────────────────────────────
    struct sigaction sa;
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    // ── Buat folder honeypot dan quarantine jika belum ada ────────────────
    mkdir(HONEYPOT_DIR,   0755);
    mkdir(QUARANTINE_DIR, 0755);

    // ── Simpan PID ke watcher.pid ─────────────────────────────────────────
    FILE *pid_fp = fopen(PID_FILE, "w");
    if (pid_fp) {
        fprintf(pid_fp, "%d\n", getpid());
        fclose(pid_fp);
    }

    // ── Buka file log ─────────────────────────────────────────────────────
    log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) exit(EXIT_FAILURE);

    write_log("Daemon watcher dimulai.");

    // ── Loop utama: polling setiap 1 detik ────────────────────────────────
    while (1) {
        DIR *dir = opendir(HONEYPOT_DIR);
        if (!dir) {
            sleep(1);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type != DT_REG) continue;

            char *fname = entry->d_name;
            char *dot   = strrchr(fname, '.');
            if (!dot) continue;

            // Cek ekstensi .exe atau .pcap
            if (strcmp(dot, ".exe") != 0 && strcmp(dot, ".pcap") != 0) continue;

            // Bangun path sumber dan tujuan
            char src[512], dst[512], log_msg[512];
            snprintf(src, sizeof(src), "%s/%s", HONEYPOT_DIR, fname);
            snprintf(dst, sizeof(dst), "%s/%s", QUARANTINE_DIR, fname);

            // Catat penemuan
            snprintf(log_msg, sizeof(log_msg),
                     "Peringatan! Menemukan file mencurigakan: %s", fname);
            write_log(log_msg);

            // Pindahkan file ke quarantine
            if (rename(src, dst) == 0) {
                snprintf(log_msg, sizeof(log_msg),
                         "Berhasil mengkarantina file: %s", fname);
                write_log(log_msg);
            }
        }
        closedir(dir);
        sleep(1);
    }

    return 0;
}
