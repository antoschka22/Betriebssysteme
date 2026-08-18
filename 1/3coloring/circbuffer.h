/**
 * @file	circbuffer.h
 * @author	Antonio Molina Gradischnig, 12421907
 * @brief	Contains definitions for all circular buffer handling functions.
 * @details	Contains definitions for all circular buffer handling functions, as well as boundaries and helper structs.
 * @date	16-11-2025
 */
#pragma once

#include "commons.h"
#include <semaphore.h>

#define	CIRC_BUF_ELEMENT_MAX_EDGE_CNT	(16)
#define	CIRC_BUF_SIZE	(60)
#define CIRC_BUF_OK		(0)
#define CIRC_BUF_ERROR	(741)

/**
 * @brief	Semaphores for operating on the circular buffer.
 * @details	Contains the semaphores used by the reader and writer processes, as well as a mutual exclusion semaphore for more than one writer process.
 */
struct circ_buffer_semaphores
{
	sem_t*	num_of_free_sem;	///< Counts the number of free spaces in the buffer.
	sem_t*	num_of_written_sem;	///< Counts the number of non-free spaces in the buffer.
	sem_t*	mutex_sem;			///< Used for mutual exclusion of writers.
};

/**
 * @brief	An element of the circular buffer.
 * @details	Holds the number of removed by a particular solution edges, as well as a list of the edges themselves (not bigger than **CIRC_BUF_ELEMENT_MAX_EDGE_CNT**).
 */
struct circ_buffer_element
{
	int					edge_cnt;	///< Number of removed edges in this particular solution.
	struct graph_edge	edges[CIRC_BUF_ELEMENT_MAX_EDGE_CNT];	///< Array of removed edges.
};

struct circ_buffer;

void circ_buf_init(struct circ_buffer* buffer);

int circ_buf_write(struct circ_buffer* buffer, struct circ_buffer_element* element, struct circ_buffer_semaphores* semaphores);

int circ_buf_read(struct circ_buffer* buffer, struct circ_buffer_element* out_element, struct circ_buffer_semaphores* semaphores);