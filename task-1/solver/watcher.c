#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>

#define HONEYPOT   "honeypot"
#define QUARANTINE "quarantine"
#define PID_FILE   "watcher.pid"
#define LOG_FILE   "security.log"

static FILE *log_fp = NULL;

static void log_write(const char *msg) {
    if (!log_fp) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log_fp, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec, msg);
    fflush(log_fp);
}

static void on_sigterm(int sig) {
    (void)sig;
    if (log_fp) fclose(log_fp);
    remove(PID_FILE);
    exit(0);
}

static int bad_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    return (strcmp(dot, ".exe") == 0 || strcmp(dot, ".pcap") == 0);
}

int main(void) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid > 0) exit(0);

    setsid();

    pid_t pid2 = fork();
    if (pid2 < 0) exit(1);
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
    if (pidf) { fprintf(pidf, "%d\n", getpid()); fclose(pidf); }

    log_fp = fopen(LOG_FILE, "a");

    signal(SIGTERM, on_sigterm);

    mkdir(HONEYPOT,   0755);
    mkdir(QUARANTINE, 0755);

    char msg[512];
    while (1) {
        DIR *dir = opendir(HONEYPOT);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type != DT_REG) continue;
                if (!bad_ext(entry->d_name)) continue;

                snprintf(msg, sizeof(msg),
                         "Peringatan! Menemukan file mencurigakan: %s", entry->d_name);
                log_write(msg);

                char src[512], dst[512];
                snprintf(src, sizeof(src), "%s/%s", HONEYPOT, entry->d_name);
                snprintf(dst, sizeof(dst), "%s/%s", QUARANTINE, entry->d_name);
                rename(src, dst);

                snprintf(msg, sizeof(msg),
                         "Berhasil mengkarantina file: %s", entry->d_name);
                log_write(msg);
            }
            closedir(dir);
        }
        sleep(1);
    }

    return 0;
}
