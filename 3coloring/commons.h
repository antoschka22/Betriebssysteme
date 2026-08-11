/**
 * @file	commons.h
 * @author	Antonio Molina Gradischnig, 12421907
 * @brief	Contains all shared definitions / structs.
 * @details	Contains all definitions / structs shared by the supervisor and generator.
 * @date	13-11-2025
 */
#pragma once

#include <unistd.h>
#include <stdint.h>

#define EF_WORKING	(1)
#define EF_DONE		(2112)
#define SHMEM_BLOCK_SIZE	(4092)
#define	SEM_FREE_NAME		"/12220025_supsemforfree"
#define	SEM_WRITTEN_NAME	"/12220025_supsemforwritten"
#define	SEM_MUTEX_NAME		"/12220025_supsemmutex"
#define SHMEM_NAME			"/12220025_sharedmemory"

/**
 * @brief	Context of a given process.
 * @details	Holds the process ID and program name (according to __arvg[0]__) of the current process.
 */
struct program_ctx
{
	pid_t pid;		///< Current process' ID.
	char* progname;	///< Current process' name (__argv[0]__).
};

/**
 * @brief	Edge of a graph.
 * @details	Holds both vertices of the edge, used to index into colouring array.
 */
struct graph_edge
{
	uint16_t	v1;	///< Vertex 1
	uint16_t	v2;	///< Vertex 2
};