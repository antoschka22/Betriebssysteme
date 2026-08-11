/**
 * @file	supervisor.c
 * @author	Antonio Molina Gradischnig, 12421907
 * @brief	Supervisor program.
 * @details	Contains the actual implementation of the supervisor.
 * @date	13-11-2025
 */
#include "supervisor.h"
#include "circbuffer.h"
#include "printdebug.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

/**
 * @brief Deallocates resources and exits the process.
 * @details Looks at the program state variable, to determine which resources should be deallocated. Notice that the 'switch' statement does not have any breaks, i.e. after an execution of a given case it moves forward with the next one.
 * @param ctx Supervisor program context; Contains "global" program variables.
 * @param exitcode Exit code of the program.
 */
void exit_sup_cleanup(struct supervisor_ctx* ctx, int exitcode)
{
	int i = 0;
	switch (ctx->prog_state)
	{
	case SSTATE_SHMEM_MAPPED:
		*(int*)(ctx->shmem_addr) = EF_DONE;
		munmap(ctx->shmem_addr, SHMEM_BLOCK_SIZE);
	case SSTATE_SHMEM_OPEN:
		close(ctx->shmem_fd);
		shm_unlink(SHMEM_NAME);
	case SSTATE_MUTEXSEM_OPEN:
		sem_close(ctx->semaphores.mutex_sem);
		sem_unlink(SEM_MUTEX_NAME);
	case SSTATE_WRITESEM_OPEN:
		for (sem_getvalue(ctx->semaphores.num_of_written_sem, &i); i > 0; i--)
		{
			sem_post(ctx->semaphores.num_of_free_sem);
		}
		sem_close(ctx->semaphores.num_of_written_sem);
		sem_unlink(SEM_WRITTEN_NAME);
	case SSTATE_FREESEM_OPEN:
		sem_close(ctx->semaphores.num_of_free_sem);
		sem_unlink(SEM_FREE_NAME);
	default:
		exit(exitcode);
		break;
	}
}

/**
 * @brief	Prints usage message to **stdout**.
 * @details	Prints the message "Usage: ..." to **stdout**, preceded by the
 * program name (argv[0]).
 * @param	prog_ctx Program context of the current process.
 */
static void print_usage_message(struct program_ctx* prog_ctx) { printf("Usage: %s [-n limit] [-w delay]\n", prog_ctx->progname); }

/// Global supervisor context. Needs to be global in order to be accessed by the
/// signal handler.
static struct supervisor_ctx ctx;

/**
 * @brief	Prints final solution.
 * @details	Prints whether the graph is 3-colourable or not to **stdout**.
 * Uses global variable _ctx_.
 */
static void print_exit_message(void)
{
	print_debug(ctx.prog_ctx, "End flag: %d", *(int*)(ctx.shmem_addr));
	if (ctx.num_removed_edges == 0)
	{
		printf("The graph is 3-colorable!\n");
		return;
	}
	printf("The graph might not be 3-colorable, best solution removes %d edges.\n", ctx.num_removed_edges);
}

/**
 * @brief	Signal handler for SIGINT and SIGTERM.
 * @details	Sets the 'end flag' of the supervisor, to indicate that
 * execution must end. Uses global variable _ctx_.
 * @param	signum Signal number.
 */
void signal_handler(int signum) { ctx.end_flag = EF_DONE; }

/**
 * @brief	Creates a semaphore and returns a pointer to it.
 * @details	Creates a semaphore using 'sem_open'. If an error is
 * encountered, the cleanup function is called. Uses global variable _ctx_.
 * @param	sem_filename Filename of the semaphore to open.
 * @param	init_val Initial value to set the semaphore to.
 * @return	Pointer to the newly created semaphore.
 */
static sem_t* open_semaphore(const char* sem_filename, unsigned int init_val)
{
	sem_t* new_sem = sem_open(sem_filename, O_CREAT | O_EXCL, 0600, init_val);
	if (new_sem == SEM_FAILED)
	{
		print_error(&(ctx.prog_ctx), sem_filename, errno);
		exit_sup_cleanup(&ctx, EXIT_FAILURE);
	}
	return new_sem;
}

/**
 * @brief	Main body of the program.
 * @details	Almost all logic of the supervisor process is contained here.
 * Uses global variable _ctx_.
 * @param	argc Number of arguments.
 * @param	argv Array of arguments.
 * @return	EXIT_FAILURE on failure, otherwise nothing (exit() is called).
 */
int main(int argc, char** argv)
{
	struct program_ctx pctx = {.pid = getpid(), .progname = argv[0]};
	ctx.prog_ctx = pctx;
	ctx.prog_state = SSTATE_START;
	ctx.iter_limit = -1;
	ctx.initial_delay = UINT_MAX;
	ctx.num_removed_edges = INT_MAX;
	ctx.end_flag = EF_WORKING;

	// DONE: Add signal handlers
	struct sigaction action;
	action.sa_handler = signal_handler;
	sigemptyset(&(action.sa_mask));
	action.sa_flags = 0;

	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	// DONE: Parse options
	opterr = 0; // To stop getopt() from printing an error
	int option = 0;
	while ((option = getopt(argc, argv, "n:w:")) != -1)
	{
		switch (option)
		{
		case 'n':
			if ((optarg == NULL) || (ctx.iter_limit > -1))
			{
				print_usage_message(&(ctx.prog_ctx));
				exit_sup_cleanup(&ctx, EXIT_FAILURE);
			}
			errno = 0;
			ctx.iter_limit = strtol(optarg, NULL, 10);
			if (errno != 0)
			{
				print_usage_message(&(ctx.prog_ctx));
				exit_sup_cleanup(&ctx, EXIT_FAILURE);
			}
			break;
		case 'w':
			if ((optarg == NULL) || (ctx.initial_delay != UINT_MAX))
			{
				print_usage_message(&(ctx.prog_ctx));
				exit_sup_cleanup(&ctx, EXIT_FAILURE);
			}
			errno = 0;
			ctx.initial_delay = strtol(optarg, NULL, 10);
			if (errno != 0)
			{
				print_usage_message(&(ctx.prog_ctx));
				exit_sup_cleanup(&ctx, EXIT_FAILURE);
			}
			break;
		default:
			print_usage_message(&(ctx.prog_ctx));
			exit_sup_cleanup(&ctx, EXIT_FAILURE);
			break;
		}
	}

	// Opening semaphores. Order matters! for error-handling function
	ctx.semaphores.num_of_free_sem = open_semaphore(SEM_FREE_NAME, CIRC_BUF_SIZE);
	ctx.prog_state = SSTATE_FREESEM_OPEN;

	ctx.semaphores.num_of_written_sem = open_semaphore(SEM_WRITTEN_NAME, 0);
	ctx.prog_state = SSTATE_WRITESEM_OPEN;

	ctx.semaphores.mutex_sem = open_semaphore(SEM_MUTEX_NAME, 1);
	ctx.prog_state = SSTATE_MUTEXSEM_OPEN;

	ctx.shmem_fd = shm_open(SHMEM_NAME, O_RDWR | O_CREAT, 0600);
	if (ctx.shmem_fd < 0)
	{
		print_error(&(ctx.prog_ctx), "Could not open " SHMEM_NAME, errno);
		exit_sup_cleanup(&ctx, EXIT_FAILURE);
	}
	ctx.prog_state = SSTATE_SHMEM_OPEN;

	if (ftruncate(ctx.shmem_fd, SHMEM_BLOCK_SIZE) < 0)
	{
		print_error(&(ctx.prog_ctx), "Could not truncate shared memory at " SHMEM_NAME, errno);
		exit_sup_cleanup(&ctx, EXIT_FAILURE);
	}

	// DONE: mmap
	ctx.shmem_addr = mmap(NULL, SHMEM_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ctx.shmem_fd, 0);
	if (ctx.shmem_addr == MAP_FAILED)
	{
		print_error(&(ctx.prog_ctx), "Could not map shared memory " SHMEM_NAME, errno);
		exit_sup_cleanup(&ctx, EXIT_FAILURE);
	}
	ctx.prog_state = SSTATE_SHMEM_MAPPED;

	*(int*)(ctx.shmem_addr) = EF_WORKING;
	ctx.buffer = (struct circ_buffer*)((int*)(ctx.shmem_addr) + 1);
	circ_buf_init(ctx.buffer);

	ctx.initial_delay = ctx.initial_delay == UINT_MAX ? 0 : ctx.initial_delay;
	if (sleep(ctx.initial_delay) != 0)
	{
		print_error(&(ctx.prog_ctx), "Did not wait the specified time", errno);
		exit_sup_cleanup(&ctx, EXIT_SUCCESS);
	}

	// DONE: for loop
	for (unsigned int i = 0; ((i < ctx.iter_limit) || (ctx.iter_limit < 0)) && (ctx.end_flag == EF_WORKING); i++)
	{
		if (circ_buf_read(ctx.buffer, &(ctx.sln), &(ctx.semaphores)) != CIRC_BUF_OK)
		{
			print_error(&(ctx.prog_ctx), "Could not read a solution from shared memory", errno);
			exit_sup_cleanup(&ctx, EXIT_FAILURE);
		}
		// DONE: Full solution print
		if (ctx.sln.edge_cnt < ctx.num_removed_edges)
		{
			ctx.num_removed_edges = ctx.sln.edge_cnt;
			if (ctx.sln.edge_cnt != 0)
			{
				fprintf(stderr, "Solution with %d edges:", ctx.sln.edge_cnt);
				for (int j = 0; j < ctx.sln.edge_cnt; j++)
				{
					fprintf(stderr, " %hu-%hu", ctx.sln.edges[j].v1, ctx.sln.edges[j].v2);
				}
				fputc('\n', stderr);
			}
			else
				break;
		}
	}

	print_exit_message();
	exit_sup_cleanup(&ctx, EXIT_SUCCESS);

	return EXIT_FAILURE;
}