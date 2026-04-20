#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_TYPES  64
#define MAX_NAME   256
#define LOGS_DIR   "logs_dump"

// ── Struktur untuk menyimpan jenis serangan unik ──────────────────────────
char attack_types[MAX_TYPES][MAX_NAME];
int  type_count = 0;

// Cek apakah jenis serangan sudah ada di array
int already_exists(const char *type) {
    for (int i = 0; i < type_count; i++) {
        if (strcmp(attack_types[i], type) == 0) return 1;
    }
    return 0;
}

// Ekstrak jenis serangan dari nama file
// Format: [Jenis Serangan]_[IP]_[Timestamp].log
// Ambil bagian sebelum underscore pertama
void extract_attack_type(const char *filename, char *out) {
    strncpy(out, filename, MAX_NAME - 1);
    out[MAX_NAME - 1] = '\0';
    char *underscore = strchr(out, '_');
    if (underscore) *underscore = '\0';
}

int main() {
    DIR    *dir;
    struct dirent *entry;
    char   attack[MAX_NAME];

    // ── Langkah 1: Scan logs_dump dan kumpulkan jenis serangan unik ───────
    dir = opendir(LOGS_DIR);
    if (!dir) {
        perror("opendir gagal");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) {
        // Lewati entry bukan file reguler dan file tersembunyi
        if (entry->d_type != DT_REG) continue;
        if (entry->d_name[0] == '.') continue;
        // Hanya proses file .log
        if (!strstr(entry->d_name, ".log")) continue;

        extract_attack_type(entry->d_name, attack);

        if (!already_exists(attack) && type_count < MAX_TYPES) {
            strncpy(attack_types[type_count], attack, MAX_NAME - 1);
            type_count++;
            printf("[INFO] Jenis serangan ditemukan: %s\n", attack);
        }
    }
    closedir(dir);

    if (type_count == 0) {
        printf("Tidak ada file log ditemukan.\n");
        return 0;
    }

    // ── Langkah 2: Buat satu child process per jenis serangan unik ────────
    // Setiap child bertanggung jawab memindahkan file milik satu jenis serangan
    for (int i = 0; i < type_count; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork gagal");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            // ── Child process ──────────────────────────────────────────────
            char dest_dir[MAX_NAME * 2];
            snprintf(dest_dir, sizeof(dest_dir), "%s/%s", LOGS_DIR, attack_types[i]);

            // Buat subfolder jika belum ada
            mkdir(dest_dir, 0755);

            // Scan ulang dan pindahkan semua file milik jenis ini
            DIR *cdir = opendir(LOGS_DIR);
            if (!cdir) {
                perror("opendir child gagal");
                exit(EXIT_FAILURE);
            }

            struct dirent *centry;
            char src_path[MAX_NAME * 4];
            char dst_path[MAX_NAME * 4];
            char file_type[MAX_NAME];

            while ((centry = readdir(cdir)) != NULL) {
                if (centry->d_type != DT_REG) continue;
                if (centry->d_name[0] == '.') continue;
                if (!strstr(centry->d_name, ".log")) continue;

                extract_attack_type(centry->d_name, file_type);

                if (strcmp(file_type, attack_types[i]) == 0) {
                    snprintf(src_path, sizeof(src_path), "%s/%s", LOGS_DIR, centry->d_name);
                    snprintf(dst_path, sizeof(dst_path), "%s/%s", dest_dir, centry->d_name);
                    rename(src_path, dst_path);
                }
            }
            closedir(cdir);

            printf("[Child PID %d] Selesai memindahkan file: %s\n", getpid(), attack_types[i]);
            exit(EXIT_SUCCESS);
        }
        // Parent tidak menunggu di sini — biarkan semua child jalan paralel
    }

    // ── Langkah 3: Parent tunggu semua child selesai ──────────────────────
    for (int i = 0; i < type_count; i++) {
        wait(NULL);
    }

    printf("[OK] Klasifikasi selesai. %d kategori dibuat.\n", type_count);
    return 0;
}
