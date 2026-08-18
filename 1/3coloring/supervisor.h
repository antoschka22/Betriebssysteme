/**
 * @file	supervisor.h
 * @author	Antonio Molina Gradischnig, 12421907
 * @brief	Contains definitions for supervisor status codes and helper structs.
 * @details	Contains definitions for all supervisor status codes, as well as the context.
 * @date	13-11-2025
 */
#pragma once

#include "circbuffer.h"
#include "commons.h"

#define SSTATE_START (1)
#define	SSTATE_GOT_OPTS			(2)
#define	SSTATE_FREESEM_OPEN		(4)
#define	SSTATE_WRITESEM_OPEN	(8)
#define	SSTATE_MUTEXSEM_OPEN	(12)
#define SSTATE_SHMEM_OPEN		(16)
#define SSTATE_SHMEM_MAPPED		(20)

/**
 * @brief	Context of a supervisor process.
 * @details	Holds all variables used by a supervisor process, to enable passing them to other functions easily.
 */
struct supervisor_ctx
{
	struct program_ctx	prog_ctx;	///< The program context of the current process.
	int					prog_state;	///< The current state of the program - how far the execution has gone.
	int 				iter_limit;	///< The limit of iterations, -1 if unlimited.
	unsigned int 		initial_delay;	///< The number of seconds to wait at the start.
	struct circ_buffer*	buffer;		///< Pointer to the circular buffer.
	struct circ_buffer_semaphores	semaphores;	///< The semaphores, used for synchronised reading of the **circ_buf**.
	int					shmem_fd;	///< File descriptor of shared memory.
	void*				shmem_addr;	///< Pointer to the start of the shared memory mapping.
	int					num_removed_edges;		///< The cnumber of removed edges in the current best solution.
	struct circ_buffer_element		sln;		///< The currently processed solution.
	short int			end_flag;	///< End flag - indicates whether the program should end execution.
};