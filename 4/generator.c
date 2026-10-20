/**
 * @file generator.c
 * @author Antonio Molina Gradischnig
 * @date 2026-20-10
 * @brief Generator program for the Feedback Arc Set problem.
 * @details Parses a directed graph from command line arguments, repeatedly
 *          generates random vertex permutations using the Fisher-Yates shuffle,
 *          computes the corresponding feedback arc set, and writes small solutions
 *          to the shared circular buffer until instructed to terminate.
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
#include <time.h>
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
 * @brief Prints the command line usage message to stderr and exits with EXIT_FAILURE.
 * @param prog_name Name of the executable (argv[0]).
 */
static void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s EDGE1...\n", prog_name);
    exit(EXIT_FAILURE);
}

/**
 * @brief Parses an edge string formatted as "u-v" into an edge_t struct.
 * @details Uses strtol to validate non-negative integer vertex indices.
 * @param str The argument string representing the edge.
 * @param edge Pointer to the edge_t structure to populate.
 * @return 0 on success, -1 if the string format or numbers are invalid.
 */
static int parse_edge(const char *str, edge_t *edge) {
    if (str == NULL || *str == '\0') {
        return -1;
    }

    char *endptr = NULL;
    errno = 0;
    long u = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '-' || u < 0 || u > INT_MAX) {
        return -1;
    }

    const char *v_str = endptr + 1;
    if (*v_str == '\0') {
        return -1;
    }

    errno = 0;
    long v = strtol(v_str, &endptr, 10);
    if (errno != 0 || endptr == v_str || *endptr != '\0' || v < 0 || v > INT_MAX) {
        return -1;
    }

    edge->u = (int)u;
    edge->v = (int)v;
    return 0;
}

/**
 * @brief Searches for a vertex ID in the unique vertices array.
 * @param vertices Array of unique vertex IDs.
 * @param count Current number of unique vertices in the array.
 * @param id Vertex ID to find.
 * @return Index of the vertex if found, or -1 if not present.
 */
static int find_vertex_index(const int *vertices, size_t count, int id) {
    for (size_t i = 0; i < count; i++) {
        if (vertices[i] == id) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Shuffles an array of vertex positions using the Fisher-Yates algorithm.
 * @param pos Array of vertex positions to shuffle in-place.
 * @param n Number of elements in the array.
 */
static void shuffle_positions(size_t *pos, size_t n) {
    if (n <= 1) {
        return;
    }
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)(rand() % (i + 1));
        size_t tmp = pos[i];
        pos[i] = pos[j];
        pos[j] = tmp;
    }
}

/**
 * @brief Main entry point of the generator program.
 * @details Parses graph edges, attaches to shared memory and semaphores created by
 *          the supervisor, generates random feedback arc sets, and writes valid
 *          sets to the circular buffer. Uses global variable @ref quit.
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 * @return EXIT_SUCCESS on normal completion, EXIT_FAILURE on error.
 */
int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "")) != -1) {
        switch (opt) {
            case '?':
                usage(argv[0]);
                break;
            default:
                assert(0);
                break;
        }
    }

    if (optind >= argc) {
        usage(argv[0]);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        fprintf(stderr, "[%s] ERROR: sigaction failed: %s\n", argv[0], strerror(errno));
        return EXIT_FAILURE;
    }

    size_t num_edges = (size_t)(argc - optind);
    edge_t *orig_edges = malloc(num_edges * sizeof(edge_t));
    edge_t *norm_edges = malloc(num_edges * sizeof(edge_t));
    int *vertices = malloc(2 * num_edges * sizeof(int));
    size_t *pos = NULL;

    if (orig_edges == NULL || norm_edges == NULL || vertices == NULL) {
        fprintf(stderr, "[%s] ERROR: malloc failed: %s\n", argv[0], strerror(errno));
        free(orig_edges);
        free(norm_edges);
        free(vertices);
        return EXIT_FAILURE;
    }

    size_t num_vertices = 0;
    for (size_t i = 0; i < num_edges; i++) {
        if (parse_edge(argv[optind + (int)i], &orig_edges[i]) != 0) {
            free(orig_edges);
            free(norm_edges);
            free(vertices);
            usage(argv[0]);
        }

        int u_idx = find_vertex_index(vertices, num_vertices, orig_edges[i].u);
        if (u_idx == -1) {
            u_idx = (int)num_vertices;
            vertices[num_vertices++] = orig_edges[i].u;
        }

        int v_idx = find_vertex_index(vertices, num_vertices, orig_edges[i].v);
        if (v_idx == -1) {
            v_idx = (int)num_vertices;
            vertices[num_vertices++] = orig_edges[i].v;
        }

        norm_edges[i].u = u_idx;
        norm_edges[i].v = v_idx;
    }

    pos = malloc(num_vertices * sizeof(size_t));
    if (pos == NULL) {
        fprintf(stderr, "[%s] ERROR: malloc failed: %s\n", argv[0], strerror(errno));
        free(orig_edges);
        free(norm_edges);
        free(vertices);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < num_vertices; i++) {
        pos[i] = i;
    }

    int ret = EXIT_FAILURE;
    int shm_fd = -1;
    shm_data_t *shm = MAP_FAILED;
    sem_t *sem_free = SEM_FAILED;
    sem_t *sem_used = SEM_FAILED;
    sem_t *sem_mutex = SEM_FAILED;

    shm_fd = shm_open(SHM_NAME, O_RDWR, 0600);
    if (shm_fd == -1) {
        fprintf(stderr, "[%s] ERROR: shm_open failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    shm = mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        fprintf(stderr, "[%s] ERROR: mmap failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    sem_free = sem_open(SEM_FREE_NAME, 0);
    if (sem_free == SEM_FAILED) {
        fprintf(stderr, "[%s] ERROR: sem_open(free) failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    sem_used = sem_open(SEM_USED_NAME, 0);
    if (sem_used == SEM_FAILED) {
        fprintf(stderr, "[%s] ERROR: sem_open(used) failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    sem_mutex = sem_open(SEM_MUTEX_NAME, 0);
    if (sem_mutex == SEM_FAILED) {
        fprintf(stderr, "[%s] ERROR: sem_open(mutex) failed: %s\n", argv[0], strerror(errno));
        goto cleanup;
    }

    /* Seed PRNG uniquely per generator process */
    srand((unsigned int)(time(NULL) ^ (getpid() << 16)));

    while (!quit && !shm->terminate) {
        shuffle_positions(pos, num_vertices);

        solution_t sol;
        sol.count = 0;
        bool too_large = false;

        for (size_t i = 0; i < num_edges; i++) {
            if (pos[norm_edges[i].u] >= pos[norm_edges[i].v]) {
                if (sol.count >= MAX_SOLUTION_EDGES) {
                    too_large = true;
                    break;
                }
                sol.edges[sol.count++] = orig_edges[i];
            }
        }

        if (too_large) {
            continue;
        }

        if (sem_wait(sem_free) == -1) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "[%s] ERROR: sem_wait(free) failed: %s\n", argv[0], strerror(errno));
            goto cleanup;
        }

        if (shm->terminate || quit) {
            sem_post(sem_free);
            break;
        }

        if (sem_wait(sem_mutex) == -1) {
            sem_post(sem_free);
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "[%s] ERROR: sem_wait(mutex) failed: %s\n", argv[0], strerror(errno));
            goto cleanup;
        }

        if (shm->terminate || quit) {
            sem_post(sem_mutex);
            sem_post(sem_free);
            break;
        }

        shm->buffer[shm->write_idx] = sol;
        shm->write_idx = (shm->write_idx + 1) % BUFFER_SIZE;

#ifdef DEBUG
        fprintf(stderr, "[%s] DEBUG: Wrote solution with %zu edges\n", argv[0], sol.count);
#endif

        if (sem_post(sem_mutex) == -1) {
            fprintf(stderr, "[%s] ERROR: sem_post(mutex) failed: %s\n", argv[0], strerror(errno));
            goto cleanup;
        }

        if (sem_post(sem_used) == -1) {
            fprintf(stderr, "[%s] ERROR: sem_post(used) failed: %s\n", argv[0], strerror(errno));
            goto cleanup;
        }
    }

    ret = EXIT_SUCCESS;

cleanup:
    if (sem_mutex != SEM_FAILED) {
        sem_close(sem_mutex);
    }
    if (sem_used != SEM_FAILED) {
        sem_close(sem_used);
    }
    if (sem_free != SEM_FAILED) {
        sem_close(sem_free);
    }
    if (shm != MAP_FAILED) {
        munmap(shm, sizeof(shm_data_t));
    }
    if (shm_fd != -1) {
        close(shm_fd);
    }
    free(pos);
    free(vertices);
    free(norm_edges);
    free(orig_edges);

    return ret;
}