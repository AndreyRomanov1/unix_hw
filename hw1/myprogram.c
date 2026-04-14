#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEFAULT_BLOCK_SIZE 4096

static int is_zero_block(const char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (buf[i] != '\0')
            return 0;
    }
    return 1;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-b block_size] [input_file] output_file\n", prog);
    fprintf(stderr, "  -b block_size  Block size in bytes (default: %d)\n", DEFAULT_BLOCK_SIZE);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int opt;
    size_t block_size = DEFAULT_BLOCK_SIZE;

    while ((opt = getopt(argc, argv, "b:")) != -1) {
        switch (opt) {
        case 'b':
            block_size = (size_t)atol(optarg);
            if (block_size == 0) {
                fprintf(stderr, "Error: invalid block size '%s'\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        default:
            usage(argv[0]);
        }
    }

    int remaining = argc - optind;
    int in_fd;
    const char *out_path;

    if (remaining == 1) {
        in_fd = STDIN_FILENO;
        out_path = argv[optind];
    } else if (remaining == 2) {
        const char *in_path = argv[optind];
        out_path = argv[optind + 1];
        in_fd = open(in_path, O_RDONLY);
        if (in_fd < 0) {
            perror("open input");
            exit(EXIT_FAILURE);
        }
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    int out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        perror("open output");
        exit(EXIT_FAILURE);
    }

    char *buf = malloc(block_size);
    if (!buf) {
        fprintf(stderr, "Error: malloc failed for block size %zu\n", block_size);
        exit(EXIT_FAILURE);
    }

    off_t total_written = 0;
    ssize_t n;

    while ((n = read(in_fd, buf, block_size)) > 0) {
        total_written += (off_t)n;
        if (is_zero_block(buf, (size_t)n)) {
            if (lseek(out_fd, (off_t)n, SEEK_CUR) < 0) {
                perror("lseek");
                exit(EXIT_FAILURE);
            }
        } else {
            if (write(out_fd, buf, (size_t)n) < 0) {
                perror("write");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (n < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(out_fd, total_written) < 0) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    free(buf);
    close(out_fd);
    if (in_fd != STDIN_FILENO)
        close(in_fd);

    return EXIT_SUCCESS;
}
