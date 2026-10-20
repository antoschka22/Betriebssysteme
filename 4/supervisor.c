/**
 * @file supervisor.c
 * @author Antonio Molina Gradischnig
 * @date 2026-20-10
 * @brief Supervisor program for the Feedback Arc Set problem.
 * @details Sets up shared memory and POSIX named semaphores, waits for generators
 *          to write feedback arc set solutions into the circular buffer, tracks
 *          the minimal solution, and coordinates graceful termination.
 */

#include "common.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Global flag indicating whether a termination signal was received.
 */
static volatile sig_atomic_t quit = 0;

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 * @details Sets the global quit flag using only async-signal-safe operations.
 *          Accesses global variable @ref quit.
 * @param sig The signal number received.
 */
static void handle_signal(int sig) {
    (void)sig;
    quit = 1;
}

/**
 * @brief Prints the command line usage message and exits.
 * @details Outputs usage syntax to stdout if status is EXIT_SUCCESS,
 *          or to stderr if status is EXIT_FAILURE.
 * @param prog_name Name of the executable (argv[0]).
 * @param status Exit code (EXIT_SUCCESS or EXIT_FAILURE).
 */
static void usage(const char *prog_name, int status) {
    FILE *out = (status == EXIT_SUCCESS) ? stdout : stderr;
    fprintf(out, "Usage: %s [-h] [-o outfile] [-n limit] [-w delay]\n", prog_name);
    exit(status);
}

/**
 * @brief Parses a non-negative integer string strictly using strtol.
 * @details Rejects negative numbers, empty strings, and trailing characters.
 * @param str The input string to parse.
 * @param val Pointer to store the parsed long value.
 * @return 0 on success, -1 on invalid format or out-of-range error.
 */
static int parse_non_negative_long(const char *str, long *val) {
    if (str == NULL || *str == '\0') {
        return -1;
    }
    char *endptr = NULL;
    errno = 0;
    long res = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '\0' || res < 0) {
        return -1;
    }
    *val = res;
    return 0;
}

/**
 * @brief Prints a newly discovered best solution to stderr.
 * @details Formats the edge list as "u-v" pairs separated by spaces.
 * @param sol Pointer to the solution structure to print.
 */
static void print_solution_stderr(const solution_t *sol) {
    fprintf(stderr, "Solution with %zu edges:", sol->count);
    for (size_t i = 0; i < sol->count; i++) {
        fprintf(stderr, " %d-%d", sol->edges[i].u, sol->edges[i].v);
    }
    fprintf(stderr, "\n");
}

/**
 * @brief Main entry point of the supervisor program.
 * @details Parses options, initializes output file and IPC resources, processes
 *          incoming solutions from generators, and performs full cleanup.
 *          Uses global variable @ref quit.
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 * @return EXIT_SUCCESS on normal completion, EXIT_FAILURE on error.
 */
int main(int argc, char *argv[]) {
    const char *outfile_path = NULL;
    long limit = -1;
    long delay = 0;
    bool has_o = false;
    bool has_n = false;
    bool has_w = false;

    int opt;
    while ((opt = getopt(argc, argv, "ho:n:w:")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0], EXIT_SUCCESS);
                break;
            case 'o':
                if (has_o) {
                    usage(argv[0], EXIT_FAILURE);
                }
                has_o = true;
                outfile_path = optarg;
                break;
            case 'n':
                if (has_n || parse_non_negative_long(optarg, &limit) != 0) {
                    usage(argv[0], EXIT_FAILURE);
                }
                has_n = true;
                break;
            case 'w':
                if (has_w || parse_non_negative_long(optarg, &delay) != 0 || delay > UINT_MAX) {
                    usage(argv[0], EXIT_FAILURE);
                }
                has_w = true;
                break;
            case '?':
                usage(argv[0], EXIT_FAILURE);
                break;
            default:
                assert(0);
                break;
        }
    }

    if (optind != argc) {
        usage(argv[0], EXIT_FAILURE);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        fprintf(stderr, "[%s] ERROR: sigaction failed: %s\n", argv[0], strerror(errno));
        return EXIT_FAILURE;
    }

    FILE *outfile = NULL;
    if (outfile_path != NULL) {
        outfile = fopen(outfile_path, "w");
        if (outfile == NULL) {
            fprintf(stderr, "[%s] ERROR: fopen(%s) failed: %s\n",
                    argv[0], outfile_path, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    /* Remove any stale IPC objects before creating new ones */
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_FREE_NAME);
    sem_unlink(SEM_USED_NAME);
    sem_unlink(SEM_MUTEX_NAME);

    int ret = EXIT_FAILURE;
    int shm_fd = -1;
    shm_data_t *shm = MAP_FAILED;
    sem_t *sem_free = SEM_FAILED;
    sem_t *sem_used = SEM_FAILED;
    sem_t *sem_mutex = SEM_FAILED;

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shm_fd == -1) {
        fprintf(stderr, "[%s] ERROR: shm_open failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    if (ftruncate(shm_fd, sizeof(shm_data_t)) == -1) {
        fprintf(stderr, "[%s] ERROR: ftruncate failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    shm = mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        fprintf(stderr, "[%s] ERROR: mmap failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    memset(shm, 0, sizeof(shm_data_t));

    sem_free = sem_open(SEM_FREE_NAME, O_CREAT | O_EXCL, 0600, BUFFER_SIZE);
    if (sem_free == SEM_FAILED) {
        fprintf(stderr, "[%s] ERROR: sem_open(free) failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    sem_used = sem_open(SEM_USED_NAME, O_CREAT | O_EXCL, 0600, 0);
    if (sem_used == SEM_FAILED) {
        fprintf(stderr, "[%s] ERROR: sem_open(used) failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0600, 1);
    if (sem_mutex == SEM_FAILED) {
        fprintf(stderr, "[%s] ERROR: sem_open(mutex) failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    if (delay > 0 && !quit) {
        sleep((unsigned int)delay);
    }

    size_t best_edges = (size_t)-1;
    long read_count = 0;

    while (!quit && (limit < 0 || read_count < limit)) {
        if (sem_wait(sem_used) == -1) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "[%s] ERROR: sem_wait(used) failed: %s\n", argv[0], strerror(errno));
            goto cleanup;
        }

        solution_t sol = shm->buffer[shm->read_idx];
        shm->read_idx = (shm->read_idx + 1) % BUFFER_SIZE;

        if (sem_post(sem_free) == -1) {
            fprintf(stderr, "[%s] ERROR: sem_post(free) failed: %s\n", argv[0], strerror(errno));
            goto cleanup;
        }

        read_count++;

        if (sol.count == 0) {
            best_edges = 0;
            break;
        }

        if (sol.count < best_edges) {
            best_edges = sol.count;
            print_solution_stderr(&sol);
        }
    }

    /* Notify generators to terminate and wake up any blocked writers */
    shm->terminate = 1;
    for (size_t i = 0; i < BUFFER_SIZE; i++) {
        sem_post(sem_free);
    }

    if (best_edges == 0) {
        printf("The graph is acyclic!\n");
        if (outfile != NULL) {
            fprintf(outfile, "The graph is acyclic!\n");
        }
    } else {
        size_t report_edges = (best_edges == (size_t)-1) ? 0 : best_edges;
        printf("The graph might not be acyclic, best solution removes %zu edges.\n", report_edges);
        if (outfile != NULL) {
            fprintf(outfile, "The graph might not be acyclic, best solution removes %zu edges.\n",
                    report_edges);
        }
    }

    ret = EXIT_SUCCESS;

cleanup:
    if (sem_mutex != SEM_FAILED) {
        sem_close(sem_mutex);
        sem_unlink(SEM_MUTEX_NAME);
    }
    if (sem_used != SEM_FAILED) {
        sem_close(sem_used);
        sem_unlink(SEM_USED_NAME);
    }
    if (sem_free != SEM_FAILED) {
        sem_close(sem_free);
        sem_unlink(SEM_FREE_NAME);
    }
    if (shm != MAP_FAILED) {
        munmap(shm, sizeof(shm_data_t));
    }
    if (shm_fd != -1) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
    }
    if (outfile != NULL) {
        if (fclose(outfile) == EOF) {
            fprintf(stderr, "[%s] ERROR: fclose failed: %s\n", argv[0], strerror(errno));
            ret = EXIT_FAILURE;
        }
    }

    return ret;
}