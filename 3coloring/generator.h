/**
 * @file	generator.h
 * @author	Antonio Molina Gradischnig, 12421907
 * @brief	Contains definitions for generator status codes and helper structs.
 * @details	Contains definitions for all generator status codes, the context, as well as the graph structure (all used in _errorhandling.h_)
 * @date	13-11-2025
 */
#pragma once

#include "circbuffer.h"
#include "commons.h"
#include <stdint.h>
#include <stdio.h>

#define GSTATE_START (1)
#define	GSTATE_EDGES_ALLOCD		(2)
#define GSTATE_COLOURS_ALLOCD	(3)
#define	GSTATE_FREESEM_OPEN		(4)
#define	GSTATE_WRITESEM_OPEN	(8)
#define	GSTATE_MUTEXSEM_OPEN	(12)
#define GSTATE_SHMEM_OPEN		(16)
#define GSTATE_SHMEM_MAPPED		(20)
#define GSTATE_URANDOM_OPEN		(24)


/**
 * @brief	Holds a representation of a graph.
 * @details	Holds the edges of a graph, its size, as well as the colouring of its vertices.
 */
struct graph
{
	int 				edge_cnt;	///< Number of edges
	int 				vert_cnt;	///< Number of vertices
	struct graph_edge*	edges;		///< Array of edges
	uint8_t*			colours;	///< Array of vertex colours
};

/**
 * @brief	Context of a generator process.
 * @details	Holds all variables used by a generator process, to enable passing them to other functions easily.
 */
struct generator_ctx
{
	struct program_ctx	prog_ctx;	///< The program context of the current process.
	int					prog_state;	///< The current state of the program - how far the execution has gone.
	struct graph		graph;		///< The graph, read from **stdin**.
	struct circ_buffer_element		element;	///< An element to write to the circular buffer.
	struct circ_buffer*	buffer;		///< Pointer to the circular buffer.
	struct circ_buffer_semaphores	semaphores;	///< The semaphores needed for synchronisation with other generators and supervisor.
	int					shmem_fd;	///< File descriptor of shared memory.
	void*				shmem_addr;	///< Pointer to the start of the memory mapping of the shared memory.
	FILE*				dev_urandom;			///< Points to _/dev/urandom_. For seeding purposes.
};