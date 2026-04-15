#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

int main(void) {
    mkdir("logs_dump", 0755);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execlp("unzip", "unzip", "-o", "evidence.zip", "-d", "logs_dump", NULL);
        perror("execlp unzip");
        exit(1);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        if (remove("evidence.zip") == 0)
            printf("[Info] evidence.zip berhasil dihapus.\n");
        else
            perror("remove");
    } else {
        fprintf(stderr, "[Error] Ekstraksi gagal.\n");
        return 1;
    }

    return 0;
}
