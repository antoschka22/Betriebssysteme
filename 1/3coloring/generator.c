/**
 * @file	generator.c
 * @author	Antonio Molina Gradischnig, 12421907
 * @brief	Generator program.
 * @details	Contains the actual implementation of the generator.
 * @date	13-11-2025
 */
#include "generator.h"
#include "commons.h"
#include "printdebug.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

/**
 * @brief Deallocates resources and exits the process.
 * @details Looks at the program state variable, to determine which resources should be deallocated. Notice that the 'switch' statement does not have any breaks, i.e. after an execution of a given case it moves forward with the next one.
 * @param ctx Generator program context; Contains "global" program variables.
 * @param exitcode Exit code of the program.
 */
void exit_gen_cleanup(struct generator_ctx* ctx, int exitcode)
{
	switch (ctx->prog_state)
	{
	case GSTATE_URANDOM_OPEN:
		fclose(ctx->dev_urandom);
	case GSTATE_SHMEM_MAPPED:
		munmap(ctx->shmem_addr, SHMEM_BLOCK_SIZE);
	case GSTATE_SHMEM_OPEN:
		close(ctx->shmem_fd);
	case GSTATE_MUTEXSEM_OPEN:
		sem_close(ctx->semaphores.mutex_sem);
	case GSTATE_WRITESEM_OPEN:
		sem_close(ctx->semaphores.num_of_written_sem);
	case GSTATE_FREESEM_OPEN:
		sem_close(ctx->semaphores.num_of_free_sem);
	case GSTATE_COLOURS_ALLOCD:
		free(ctx->graph.colours);
	case GSTATE_EDGES_ALLOCD:
		free(ctx->graph.edges);
	default:
		exit(exitcode);
		break;
	}
}

/**
 * @brief	Returns a random colour.
 * @details	Generates a random 'colour' (value between 0 and 2) using drand48() and some rounding trickery.
 * @return	A new random colour.
 */
static uint8_t get_color(void)
{
	float rnd = drand48();
	rnd *= 3;
	rnd = floor(rnd);
	return (uint8_t)rnd;
}

/**
 * @brief	Prints usage message to **stdout**.
 * @details	Prints the message "Usage: ..." to **stdout**, preceded by the program name (argv[0]).
 * @param	prog_ctx Program context of the current process.
 */
static void print_usage_message(struct program_ctx* prog_ctx) { printf("Usage: %s EDGE1 ...\n", prog_ctx->progname); }

/**
 * @brief	Opens a semaphore and returns a pointer to it.
 * @details	Opens an already existing semaphore using 'sem_open'. If an error is encountered, the cleanup function is called.
 * @param	sem_filename Filename of the semaphore to open.
 * @param	ctx The generator context (to be passed to cleanup function in case of error).
 * @return	Pointer to the opened semaphore.
 */
static sem_t* open_semaphore(struct generator_ctx* ctx, const char* sem_filename)
{
	sem_t* new_sem = sem_open(sem_filename, 0);
	if (new_sem == SEM_FAILED)
	{
		print_error(&(ctx->prog_ctx), sem_filename, errno);
		exit_gen_cleanup(ctx, EXIT_FAILURE);
	}
	return new_sem;
}

/**
 * @brief	Main body of the program.
 * @details	Almost all logic of the generator process is contained here.
 * @param	argc Number of arguments.
 * @param	argv Array of arguments.
 * @return	EXIT_FAILURE on failure, otherwise nothing (exit() is called).
 */
int main(int argc, char** argv)
{
	struct generator_ctx ctx;
	struct program_ctx pctx = {.pid = getpid(), .progname = argv[0]};
	ctx.prog_ctx = pctx;
	ctx.prog_state = GSTATE_START;
	ctx.dev_urandom = NULL;

	//DONE: Check for correct usage
	if (argc < 2)
	{
		print_usage_message(&pctx);
		exit_gen_cleanup(&ctx, EXIT_FAILURE);
	}

	//DONE: Parse graph
	ctx.graph.edge_cnt = argc - 1;
	ctx.graph.vert_cnt = 0;
	ctx.graph.edges = calloc(ctx.graph.edge_cnt, sizeof(struct graph_edge));
	if (ctx.graph.edges == NULL)
	{
		print_error(&(ctx.prog_ctx), "Could not allocate memory for edges", errno);
		exit_gen_cleanup(&ctx, EXIT_FAILURE);
	}
	ctx.prog_state = GSTATE_EDGES_ALLOCD;
	print_debug(ctx.prog_ctx, "Size of circ_buf_element: %lu", sizeof(struct circ_buffer_element));
	for (int i = 0; i < ctx.graph.edge_cnt; i++)
	{
		int numscanned = sscanf(argv[i + 1], "%hu-%hu", &(ctx.graph.edges[i].v1), &(ctx.graph.edges[i].v2));
		if (numscanned == EOF)
		{
			print_error(&(ctx.prog_ctx), "Could not parse an edge", errno);
			exit_gen_cleanup(&ctx, EXIT_FAILURE);
		}
		if (numscanned < 2)
		{
			print_debug(ctx.prog_ctx, "Num of scanned items: %d | %hu-%hu", numscanned, ctx.graph.edges[i].v1, ctx.graph.edges[i].v2);
			print_usage_message(&(ctx.prog_ctx));
			exit_gen_cleanup(&ctx, EXIT_FAILURE);
		}
		ctx.graph.vert_cnt = ctx.graph.edges[i].v1 > ctx.graph.vert_cnt ? ctx.graph.edges[i].v1 : ctx.graph.vert_cnt;
		ctx.graph.vert_cnt = ctx.graph.edges[i].v2 > ctx.graph.vert_cnt ? ctx.graph.edges[i].v2 : ctx.graph.vert_cnt;
		print_debug(ctx.prog_ctx, "Max vert found so far: %d", ctx.graph.vert_cnt);
	}
	ctx.graph.vert_cnt++;
	ctx.graph.colours = calloc(ctx.graph.vert_cnt, sizeof(uint8_t));
	if (ctx.graph.colours == NULL)
	{
		print_error(&(ctx.prog_ctx), "Could not allocate memory for colours", errno);
		exit_gen_cleanup(&ctx, EXIT_FAILURE);
	}
	ctx.prog_state = GSTATE_COLOURS_ALLOCD;

	//Opening semaphores. Order matters! for error-handling function
	ctx.semaphores.num_of_free_sem = open_semaphore(&ctx, SEM_FREE_NAME);
	ctx.prog_state = GSTATE_FREESEM_OPEN;

	ctx.semaphores.num_of_written_sem = open_semaphore(&ctx, SEM_WRITTEN_NAME);
	ctx.prog_state = GSTATE_WRITESEM_OPEN;

	ctx.semaphores.mutex_sem = open_semaphore(&ctx, SEM_MUTEX_NAME);
	ctx.prog_state = GSTATE_MUTEXSEM_OPEN;

	ctx.shmem_fd = shm_open(SHMEM_NAME, O_RDWR, 0600);
	if (ctx.shmem_fd < 0)
	{
		print_error(&(ctx.prog_ctx), "Could not open " SHMEM_NAME, errno);
		exit_gen_cleanup(&ctx, EXIT_FAILURE);
	}
	ctx.prog_state = GSTATE_SHMEM_OPEN;

	ctx.shmem_addr = mmap(NULL, SHMEM_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ctx.shmem_fd, 0);
	if (ctx.shmem_addr == MAP_FAILED)
	{
		print_error(&(ctx.prog_ctx), "Could not map shared memory " SHMEM_NAME, errno);
		exit_gen_cleanup(&ctx, EXIT_FAILURE);
	}
	ctx.prog_state = GSTATE_SHMEM_MAPPED;

	ctx.buffer = (struct circ_buffer*)((int*)(ctx.shmem_addr) + 1);

	ctx.dev_urandom = fopen("/dev/urandom", "r");
	if (ctx.dev_urandom == NULL)
	{
		print_error(&(ctx.prog_ctx), "Could not open '/dev/urandom'", errno);
		exit_gen_cleanup(&ctx, EXIT_FAILURE);
	}
	ctx.prog_state = GSTATE_URANDOM_OPEN;

	unsigned short int seed16[3];
	if (fread((void*)seed16, sizeof(unsigned short int), 3, ctx.dev_urandom) != 3)
	{
		print_error(&(ctx.prog_ctx), "Could not read random seed", errno);
		exit_gen_cleanup(&ctx, EXIT_FAILURE);
	}
	seed48(seed16);

	struct circ_buffer_element sln;
	uint8_t colour1 = 0, colour2 = 0;
	while (*(int*)(ctx.shmem_addr) != EF_DONE)
	{
		sln.edge_cnt = 0;
		//Generate random colouring
		for (int i = 0; i < ctx.graph.vert_cnt; i++)
		{
			ctx.graph.colours[i] = get_color();
		}

		//Check which edges need to be removed
		for (int i = 0; i < ctx.graph.edge_cnt; i++)
		{
			colour1 = ctx.graph.colours[ctx.graph.edges[i].v1];
			colour2 = ctx.graph.colours[ctx.graph.edges[i].v2];
			if (colour1 == colour2)
			{
				if (sln.edge_cnt >= CIRC_BUF_ELEMENT_MAX_EDGE_CNT)
				{
					sln.edge_cnt = -1;
					break;
				}
				sln.edges[sln.edge_cnt] = ctx.graph.edges[i];
				sln.edge_cnt++;
				print_debug(ctx.prog_ctx, "Added edge %hu-%hu to the solution; it now has %d members", ctx.graph.edges[i].v1, ctx.graph.edges[i].v2,
							sln.edge_cnt);
			}
		}
		if (sln.edge_cnt == -1)
		{
			print_debug(ctx.prog_ctx, "Generated a solution with more than %d edges", CIRC_BUF_ELEMENT_MAX_EDGE_CNT);
			continue;
		}
		if (circ_buf_write(ctx.buffer, &sln, &(ctx.semaphores)) != CIRC_BUF_OK)
		{
			print_error(&(ctx.prog_ctx), "Could not write to the shared buffer", errno);
			exit_gen_cleanup(&ctx, EXIT_FAILURE);
		}
	}

	print_debug(ctx.prog_ctx, "Exitting because END FLAG was set to %d", *(int*)(ctx.shmem_addr));
	exit_gen_cleanup(&ctx, EXIT_SUCCESS);

	return EXIT_FAILURE;
}