#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid;
    int status;

    // ── Langkah 1: Ekstrak evidence.zip ke logs_dump ──────────────────────
    // fork() membuat child process untuk menjalankan unzip
    pid = fork();
    if (pid < 0) {
        perror("fork gagal");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Child process: jalankan unzip
        // unzip -o  : overwrite tanpa tanya
        // -d        : tentukan direktori tujuan
        execlp("unzip", "unzip", "-o", "evidence.zip", "-d", "logs_dump", NULL);
        // Jika execlp gagal
        perror("execlp unzip gagal");
        exit(EXIT_FAILURE);
    }

    // Parent process: tunggu child selesai sebelum lanjut (SEKUENSIAL)
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Ekstraksi gagal dengan kode: %d\n", WEXITSTATUS(status));
        exit(EXIT_FAILURE);
    }
    printf("[OK] Ekstraksi selesai.\n");

    // ── Langkah 2: Hapus evidence.zip ─────────────────────────────────────
    // fork() lagi untuk menjalankan rm
    pid = fork();
    if (pid < 0) {
        perror("fork gagal");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Child process: hapus file zip
        execlp("rm", "rm", "evidence.zip", NULL);
        perror("execlp rm gagal");
        exit(EXIT_FAILURE);
    }

    // Parent tunggu penghapusan selesai
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Penghapusan gagal dengan kode: %d\n", WEXITSTATUS(status));
        exit(EXIT_FAILURE);
    }
    printf("[OK] evidence.zip berhasil dihapus.\n");

    return 0;
}
