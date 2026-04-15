#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOGS_DIR    "logs_dump"
#define REPORT_FILE "threat_report.txt"
#define ARCHIVE     "backup_evidence.zip"
#define MAX_CATS    64
#define MAX_IPS     1024
#define MAX_NAME    256

typedef struct { char name[MAX_NAME]; int count; } Entry;

static Entry cats[MAX_CATS];
static int   cat_count = 0;
static Entry ips[MAX_IPS];
static int   ip_count  = 0;

static int find_entry(Entry *arr, int len, const char *key) {
    for (int i = 0; i < len; i++)
        if (strcmp(arr[i].name, key) == 0) return i;
    return -1;
}

static void add_entry(Entry *arr, int *len, int max, const char *key) {
    int idx = find_entry(arr, *len, key);
    if (idx >= 0) { arr[idx].count++; return; }
    if (*len < max) {
        strncpy(arr[*len].name, key, MAX_NAME - 1);
        arr[(*len)++].count = 1;
    }
}

static int cmp_desc(const void *a, const void *b) {
    return ((Entry *)b)->count - ((Entry *)a)->count;
}

int main(void) {
    DIR *dir = opendir(LOGS_DIR);
    if (!dir) { perror("opendir logs_dump"); return 1; }

    struct dirent *cat_entry;
    while ((cat_entry = readdir(dir)) != NULL) {
        if (cat_entry->d_type != DT_DIR) continue;
        if (cat_entry->d_name[0] == '.') continue;

        char cat_path[512];
        snprintf(cat_path, sizeof(cat_path), "%s/%s", LOGS_DIR, cat_entry->d_name);

        DIR *sub = opendir(cat_path);
        if (!sub) continue;

        struct dirent *file;
        while ((file = readdir(sub)) != NULL) {
            if (file->d_type != DT_REG) continue;

            add_entry(cats, &cat_count, MAX_CATS, cat_entry->d_name);

            char fname[MAX_NAME];
            strncpy(fname, file->d_name, MAX_NAME - 1);

            char *p1 = strchr(fname, '_');
            if (!p1) continue;
            char *p2 = strchr(p1 + 1, '_');
            if (!p2) continue;

            int ip_len = p2 - (p1 + 1);
            char ip[MAX_NAME];
            strncpy(ip, p1 + 1, ip_len);
            ip[ip_len] = '\0';
            add_entry(ips, &ip_count, MAX_IPS, ip);
        }
        closedir(sub);
    }
    closedir(dir);

    qsort(cats, cat_count, sizeof(Entry), cmp_desc);
    qsort(ips,  ip_count,  sizeof(Entry), cmp_desc);

    FILE *fp = fopen(REPORT_FILE, "w");
    if (!fp) { perror("fopen threat_report.txt"); return 1; }

    fprintf(fp, "Laporan Klasifikasi Serangan:\n");
    for (int i = 0; i < cat_count; i++)
        fprintf(fp, "%d. %s muncul sebanyak %d kali.\n",
                i + 1, cats[i].name, cats[i].count);

    fprintf(fp, "\nTop 5 IP Mencurigakan:\n");
    int top = ip_count < 5 ? ip_count : 5;
    for (int i = 0; i < top; i++)
        fprintf(fp, "%d. %s muncul sebanyak %d kali.\n",
                i + 1, ips[i].name, ips[i].count);

    fclose(fp);
    printf("[Info] threat_report.txt berhasil dibuat.\n");

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        execlp("zip", "zip", "-r", ARCHIVE, LOGS_DIR, REPORT_FILE, NULL);
        perror("execlp zip");
        exit(1);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        printf("[Info] Laporan dan arsip berhasil dibuat\n");
    else
        fprintf(stderr, "[Error] Pengarsipan gagal.\n");

    return 0;
}
