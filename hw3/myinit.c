#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>

#define MAX_PROCS 64
#define MAX_ARGS  32
#define LOG_PATH  "/tmp/myinit.log"
#define PID_PATH  "/tmp/myinit.pid"

typedef struct {
    char  *args[MAX_ARGS + 1];
    char   in[PATH_MAX];
    char   out[PATH_MAX];
    pid_t  pid;
} proc_t;

static proc_t  procs[MAX_PROCS];
static int     nprocs  = 0;
static int     logfd   = -1;
static char    cfgpath[PATH_MAX];

static volatile sig_atomic_t got_sigchld = 0;
static volatile sig_atomic_t got_sighup  = 0;
static volatile sig_atomic_t got_sigterm = 0;

static void sigchld_handler(int s) { (void)s; got_sigchld = 1; }
static void sighup_handler(int s)  { (void)s; got_sighup  = 1; }
static void sigterm_handler(int s) { (void)s; got_sigterm = 1; }

static void logmsg(const char *msg) {
    char ts[20], buf[512];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
    int len = snprintf(buf, sizeof(buf), "[%s] %s\n", ts, msg);
    write(logfd, buf, len);
}

static void daemonize(void) {
    if (getppid() != 1) {
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);

        if (fork() != 0)
            exit(EXIT_SUCCESS);

        setsid();
    }

    struct rlimit flim;
    getrlimit(RLIMIT_NOFILE, &flim);
    for (int fd = 0; fd < (int)flim.rlim_max; fd++)
        close(fd);

    chdir("/");

    open("/dev/null", O_RDWR);
    dup2(0, 1);
    dup2(0, 2);

    logfd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (logfd < 0) exit(EXIT_FAILURE);

    int pfd = open(PID_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (pfd >= 0) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
        write(pfd, buf, len);
        close(pfd);
    }
}

static void free_procs(void) {
    for (int i = 0; i < nprocs; i++) {
        for (int j = 0; procs[i].args[j]; j++)
            free(procs[i].args[j]);
        procs[i].args[0] = NULL;
    }
    nprocs = 0;
}

static int parse_config(void) {
    FILE *f = fopen(cfgpath, "r");
    if (!f) { logmsg("ERROR: cannot open config"); return -1; }

    char line[1024];
    while (fgets(line, sizeof(line), f) && nprocs < MAX_PROCS) {
        char *tok[MAX_ARGS + 3];
        int n = 0;
        char *p = strtok(line, " \t\n");
        while (p && n < (int)(MAX_ARGS + 2))
            tok[n++] = p, p = strtok(NULL, " \t\n");
        if (n < 3) continue;

        if (tok[0][0] != '/' || tok[n-2][0] != '/' || tok[n-1][0] != '/') {
            char msg[256];
            snprintf(msg, sizeof(msg), "WARNING: skipping non-absolute path: %s", tok[0]);
            logmsg(msg);
            continue;
        }

        proc_t *e = &procs[nprocs++];
        for (int i = 0; i < n - 2; i++)
            e->args[i] = strdup(tok[i]);
        e->args[n - 2] = NULL;
        strncpy(e->in,  tok[n-2], PATH_MAX - 1);
        strncpy(e->out, tok[n-1], PATH_MAX - 1);
        e->pid = -1;
    }
    fclose(f);
    return 0;
}

static void spawn(int i) {
    pid_t pid = fork();
    if (pid < 0) { logmsg("ERROR: fork failed"); return; }
    if (pid == 0) {
        int fd = open(procs[i].in, O_RDONLY);
        if (fd < 0) fd = open("/dev/null", O_RDONLY);
        dup2(fd, 0); close(fd);

        fd = open(procs[i].out, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) fd = open("/dev/null", O_WRONLY);
        dup2(fd, 1); close(fd);

        close(logfd);
        execv(procs[i].args[0], procs[i].args);
        exit(EXIT_FAILURE);
    }
    procs[i].pid = pid;
    char msg[256];
    snprintf(msg, sizeof(msg), "started pid=%d cmd=%s", pid, procs[i].args[0]);
    logmsg(msg);
}

static void handle_sigchld(void) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        char msg[256];
        if (WIFEXITED(status))
            snprintf(msg, sizeof(msg), "pid=%d exited (code=%d)", pid, WEXITSTATUS(status));
        else
            snprintf(msg, sizeof(msg), "pid=%d killed (signal=%d)", pid, WTERMSIG(status));
        logmsg(msg);

        for (int i = 0; i < nprocs; i++) {
            if (procs[i].pid == pid) {
                procs[i].pid = -1;
                spawn(i);
                break;
            }
        }
    }
}

static void handle_sighup(void) {
    logmsg("SIGHUP: reloading config");

    for (int i = 0; i < nprocs; i++)
        if (procs[i].pid > 0)
            kill(procs[i].pid, SIGTERM);

    for (int i = 0; i < nprocs; i++) {
        if (procs[i].pid > 0) {
            int status;
            waitpid(procs[i].pid, &status, 0);
            char msg[256];
            snprintf(msg, sizeof(msg), "pid=%d terminated", procs[i].pid);
            logmsg(msg);
            procs[i].pid = -1;
        }
    }

    free_procs();
    if (parse_config() < 0) return;
    for (int i = 0; i < nprocs; i++)
        spawn(i);
    got_sigchld = 0;
}

static void handle_sigterm(void) {
    logmsg("SIGTERM: shutting down");
    for (int i = 0; i < nprocs; i++)
        if (procs[i].pid > 0)
            kill(procs[i].pid, SIGTERM);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "")) != -1) {
        fprintf(stderr, "Usage: %s config_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (optind >= argc) {
        fprintf(stderr, "Usage: %s config_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (argv[optind][0] != '/') {
        fprintf(stderr, "Error: config path must be absolute\n");
        exit(EXIT_FAILURE);
    }
    strncpy(cfgpath, argv[optind], PATH_MAX - 1);

    daemonize();
    logmsg("myinit started");

    signal(SIGCHLD, sigchld_handler);
    signal(SIGHUP,  sighup_handler);
    signal(SIGTERM, sigterm_handler);

    if (parse_config() < 0)
        exit(EXIT_FAILURE);

    for (int i = 0; i < nprocs; i++)
        spawn(i);

    for (;;) {
        pause();
        if (got_sigterm) { got_sigterm = 0; handle_sigterm(); }
        if (got_sighup)  { got_sighup  = 0; handle_sighup(); }
        if (got_sigchld) { got_sigchld = 0; handle_sigchld(); }
    }

    return EXIT_SUCCESS;
}
