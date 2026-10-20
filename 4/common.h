/**
 * @file common.h
 * @author Antonio Molina Gradischnig
 * @date 2026-20-10
 * @brief Common definitions, constants, and data structures for supervisor and generator.
 * @details Defines shared memory and semaphore names, circular buffer size limits,
 *          and data structures used for inter-process communication.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

/** @brief Shared memory object name (prefixed with matriculation number). */
#define SHM_NAME "/12345678_fas_shm"

/** @brief Semaphore tracking free slots in the circular buffer. */
#define SEM_FREE_NAME "/12345678_fas_sem_free"

/** @brief Semaphore tracking used slots in the circular buffer. */
#define SEM_USED_NAME "/12345678_fas_sem_used"

/** @brief Mutex semaphore protecting concurrent writes by generators. */
#define SEM_MUTEX_NAME "/12345678_fas_sem_mutex"

/** @brief Maximum number of edges stored in a single feedback arc set solution. */
#define MAX_SOLUTION_EDGES (8)

/** @brief Capacity of the circular buffer (total SHM size ~ 2608 bytes <= 4 KiB). */
#define BUFFER_SIZE (38)

/**
 * @brief Represents a directed edge from u to v.
 */
typedef struct edge {
    int u; /**< Source vertex index */
    int v; /**< Destination vertex index */
} edge_t;

/**
 * @brief Represents a single feedback arc set solution in the circular buffer.
 */
typedef struct solution {
    size_t count;                      /**< Number of edges in this solution */
    edge_t edges[MAX_SOLUTION_EDGES];  /**< Array of edges removed */
} solution_t;

/**
 * @brief Shared memory layout containing the circular buffer and state flags.
 */
typedef struct shm_data {
    volatile int terminate;            /**< Flag set to 1 by supervisor to stop generators */
    size_t write_idx;                  /**< Next index to write in the circular buffer */
    size_t read_idx;                   /**< Next index to read in the circular buffer */
    solution_t buffer[BUFFER_SIZE];    /**< Circular buffer array of solutions */
} shm_data_t;

#endif /* COMMON_H */