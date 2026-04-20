#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>

#define LOGS_DIR     "logs_dump"
#define REPORT_FILE  "threat_report.txt"
#define MAX_ENTRIES  256
#define MAX_NAME     256

// ── Struktur untuk menyimpan hitungan ─────────────────────────────────────
typedef struct {
    char name[MAX_NAME];
    int  count;
} Entry;

Entry attacks[MAX_ENTRIES];
int   attack_count = 0;

Entry ips[MAX_ENTRIES];
int   ip_count = 0;

// ── Cari atau tambah entry di array ───────────────────────────────────────
void increment(Entry *arr, int *cnt, const char *key) {
    for (int i = 0; i < *cnt; i++) {
        if (strcmp(arr[i].name, key) == 0) {
            arr[i].count++;
            return;
        }
    }
    if (*cnt < MAX_ENTRIES) {
        strncpy(arr[*cnt].name, key, MAX_NAME - 1);
        arr[*cnt].count = 1;
        (*cnt)++;
    }
}

// ── Comparator untuk qsort (descending) ──────────────────────────────────
int cmp_desc(const void *a, const void *b) {
    return ((Entry *)b)->count - ((Entry *)a)->count;
}

// ── Ekstrak jenis serangan dari nama file ─────────────────────────────────
// Format: [Jenis]_[IP]_[Timestamp].log → ambil sebelum '_' pertama
void extract_attack(const char *fname, char *out) {
    strncpy(out, fname, MAX_NAME - 1);
    out[MAX_NAME - 1] = '\0';
    char *p = strchr(out, '_');
    if (p) *p = '\0';
}

// ── Ekstrak IP dari nama file ─────────────────────────────────────────────
// Format: [Jenis]_[IP]_[Timestamp].log → ambil antara '_' pertama dan kedua
void extract_ip(const char *fname, char *out) {
    const char *first = strchr(fname, '_');
    if (!first) { out[0] = '\0'; return; }
    first++; // lewati underscore pertama

    const char *second = strchr(first, '_');
    if (!second) { out[0] = '\0'; return; }

    size_t len = second - first;
    if (len >= MAX_NAME) len = MAX_NAME - 1;
    strncpy(out, first, len);
    out[len] = '\0';
}

int main() {
    // ── Langkah 1: Baca tiap subfolder dan file di dalamnya ───────────────
    DIR *top = opendir(LOGS_DIR);
    if (!top) {
        perror("opendir logs_dump gagal");
        exit(EXIT_FAILURE);
    }

    struct dirent *cat_entry;
    while ((cat_entry = readdir(top)) != NULL) {
        // Hanya proses direktori (kategori serangan)
        if (cat_entry->d_type != DT_DIR) continue;
        if (cat_entry->d_name[0] == '.') continue;

        char cat_path[512];
        snprintf(cat_path, sizeof(cat_path), "%s/%s", LOGS_DIR, cat_entry->d_name);

        DIR *cat_dir = opendir(cat_path);
        if (!cat_dir) continue;

        struct dirent *file_entry;
        while ((file_entry = readdir(cat_dir)) != NULL) {
            if (file_entry->d_type != DT_REG) continue;
            if (!strstr(file_entry->d_name, ".log")) continue;

            char attack[MAX_NAME], ip[MAX_NAME];
            extract_attack(file_entry->d_name, attack);
            extract_ip(file_entry->d_name, ip);

            increment(attacks, &attack_count, attack);
            if (strlen(ip) > 0) increment(ips, &ip_count, ip);
        }
        closedir(cat_dir);
    }
    closedir(top);

    // ── Langkah 2: Urutkan descending ─────────────────────────────────────
    qsort(attacks, attack_count, sizeof(Entry), cmp_desc);
    qsort(ips,     ip_count,     sizeof(Entry), cmp_desc);

    // ── Langkah 3: Tulis threat_report.txt ───────────────────────────────
    FILE *fp = fopen(REPORT_FILE, "w");
    if (!fp) {
        perror("fopen threat_report.txt gagal");
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "Laporan Klasifikasi Serangan:\n");
    for (int i = 0; i < attack_count; i++) {
        fprintf(fp, "%d. %s muncul sebanyak %d kali.\n",
                i + 1, attacks[i].name, attacks[i].count);
    }

    fprintf(fp, "\nTop 5 IP Mencurigakan:\n");
    int top5 = ip_count < 5 ? ip_count : 5;
    for (int i = 0; i < top5; i++) {
        fprintf(fp, "%d. %s muncul sebanyak %d kali.\n",
                i + 1, ips[i].name, ips[i].count);
    }
    fclose(fp);
    printf("[OK] threat_report.txt berhasil dibuat.\n");

    // ── Langkah 4: Child process untuk zip ───────────────────────────────
    // fork() untuk menjalankan zip tanpa system()
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork gagal");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Child: zip logs_dump/ dan threat_report.txt menjadi backup_evidence.zip
        // -r : rekursif (ikutkan semua isi subfolder)
        execlp("zip", "zip", "-r", "backup_evidence.zip",
               LOGS_DIR, REPORT_FILE, NULL);
        perror("execlp zip gagal");
        exit(EXIT_FAILURE);
    }

    // Parent: tunggu child selesai
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("[Info] Laporan dan arsip berhasil dibuat\n");
    } else {
        fprintf(stderr, "[Error] Proses zip gagal.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
