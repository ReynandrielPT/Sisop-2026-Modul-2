#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_TYPES 64
#define MAX_NAME  128
#define LOGS_DIR  "logs_dump"

static char types[MAX_TYPES][MAX_NAME];
static int  type_count = 0;

static int find_type(const char *name) {
    for (int i = 0; i < type_count; i++)
        if (strcmp(types[i], name) == 0) return i;
    return -1;
}

static void add_type(const char *name) {
    if (find_type(name) < 0 && type_count < MAX_TYPES)
        strncpy(types[type_count++], name, MAX_NAME - 1);
}

static void move_files(const char *type) {
    char dest[256];
    snprintf(dest, sizeof(dest), "%s/%s", LOGS_DIR, type);
    mkdir(dest, 0755);

    DIR *dir = opendir(LOGS_DIR);
    if (!dir) { perror("opendir"); return; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;

        char *under = strchr(entry->d_name, '_');
        if (!under) continue;

        int tlen = under - entry->d_name;
        char found[MAX_NAME];
        strncpy(found, entry->d_name, tlen);
        found[tlen] = '\0';

        if (strcmp(found, type) != 0) continue;

        char src_path[512], dst_path[512];
        snprintf(src_path, sizeof(src_path), "%s/%s", LOGS_DIR, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s/%s", LOGS_DIR, type, entry->d_name);
        rename(src_path, dst_path);
    }
    closedir(dir);
}

int main(void) {
    DIR *dir = opendir(LOGS_DIR);
    if (!dir) {
        perror("opendir logs_dump");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;

        char *under = strchr(entry->d_name, '_');
        if (!under) continue;

        int tlen = under - entry->d_name;
        char type[MAX_NAME];
        strncpy(type, entry->d_name, tlen);
        type[tlen] = '\0';
        add_type(type);
    }
    closedir(dir);

    pid_t pids[MAX_TYPES];
    for (int i = 0; i < type_count; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); continue; }
        if (pid == 0) {
            move_files(types[i]);
            printf("[Child] Selesai mengelompokkan: %s\n", types[i]);
            exit(0);
        }
        pids[i] = pid;
    }

    for (int i = 0; i < type_count; i++)
        waitpid(pids[i], NULL, 0);

    printf("[Info] Klasifikasi selesai. %d jenis serangan ditemukan.\n", type_count);
    return 0;
}
