#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>

#define STATS_FILE "stats.txt"
#define RETRY_US   10000

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

static int lock_file(const char *lck_path) {
    char buf[32];
    int fd = open(lck_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0)
        return -1;
    int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    if (write(fd, buf, len) != len) {
        close(fd);
        unlink(lck_path);
        return -1;
    }
    close(fd);
    return 0;
}

static int unlock_file(const char *lck_path) {
    char buf[32];
    int fd = open(lck_path, O_RDONLY);
    if (fd < 0)
        return -1;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;
    buf[n] = '\0';
    if ((pid_t)atoi(buf) != getpid()) {
        fprintf(stderr, "pid %d: lock stolen\n", (int)getpid());
        return -1;
    }
    return unlink(lck_path);
}

int main(int argc, char *argv[]) {
    int opt;
    int sleep_sec = 1;

    while ((opt = getopt(argc, argv, "s:")) != -1) {
        switch (opt) {
        case 's':
            sleep_sec = atoi(optarg);
            if (sleep_sec <= 0) {
                fprintf(stderr, "Error: invalid sleep value '%s'\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "Usage: %s [-s seconds] filename\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Usage: %s [-s seconds] filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char lck_path[PATH_MAX];
    snprintf(lck_path, sizeof(lck_path), "%s.lck", argv[optind]);

    signal(SIGINT, handle_sigint);

    int lock_count = 0;

    while (running) {
        if (lock_file(lck_path) < 0) {
            usleep(RETRY_US);
            continue;
        }

        sleep(sleep_sec);
        lock_count++;

        if (unlock_file(lck_path) < 0)
            fprintf(stderr, "pid %d: unlock failed\n", (int)getpid());

        usleep(RETRY_US);
    }

    int fd = open(STATS_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        char line[64];
        int len = snprintf(line, sizeof(line), "pid %d: %d locks\n", (int)getpid(), lock_count);
        write(fd, line, len);
        close(fd);
    }

    return EXIT_SUCCESS;
}
