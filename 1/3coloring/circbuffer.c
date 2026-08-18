/**
 * @file	circbuffer.c
 * @author	Antonio Molina Gradischnig, 12421907
 * @brief	Implements all circular buffer handling functions.
 * @details	Contains a detailled description of the struct for the circular buffer, as well as the implementation of all functions from _circbuffer.h_.
 * @date	16-11-2025
 */
#include "circbuffer.h"
#include <string.h>

/**
 * @brief	The circular buffer.
 * @details	Holds all information needed to handle the circular buffer.
 */
struct circ_buffer
{
	int		first_empty_ind;	///< Index of first element that can be written to.
	int		first_written_ind;	///< Index of first element that can be read.
	struct circ_buffer_element	elements[CIRC_BUF_SIZE];	///< Array holding the actual circular buffer elements.
};

/**
 * @brief	Initializes the circular buffer.
 * @details	Sets the memory pointed to by _buffer_ to all zeroes, so as to avoid unexpected behaviour.
 * @param	buffer Pointer to the circular buffer to be initialized.
 */
void circ_buf_init(struct circ_buffer* buffer)
{
	memset((void*)buffer, 0, sizeof(struct circ_buffer));
}

/**
 * @brief	Writes an element to a circular buffer.
 * @details	Writes the element pointed to by _element_ to the buffer pointed to by _buffer_. Does not change the element.
 * @param	buffer Pointer to the circular buffer.
 * @param	element Pointer to the element to be written.
 * @param	semaphores Pointer to a collection of semaphores used for synchronization.
 * @return	**CIRC_BUF_OK** on successful write, **CIRC_BUF_ERROR** if an error is encountered.
 */
int circ_buf_write(struct circ_buffer* buffer, struct circ_buffer_element* element, struct circ_buffer_semaphores* semaphores)
{
	if(sem_wait(semaphores->num_of_free_sem) < 0) return CIRC_BUF_ERROR;
	if(sem_wait(semaphores->mutex_sem) < 0) return CIRC_BUF_ERROR;
	
	memmove((void*)&(buffer->elements[buffer->first_empty_ind]), (void*)element, sizeof(struct circ_buffer_element));
	buffer->first_empty_ind = ((buffer->first_empty_ind) + 1) % CIRC_BUF_SIZE;
	
	if(sem_post(semaphores->mutex_sem) < 0) return CIRC_BUF_ERROR;
	if(sem_post(semaphores->num_of_written_sem) < 0) return CIRC_BUF_ERROR;

	return CIRC_BUF_OK;
}

/**
 * @brief	Reads an element from a circular buffer.
 * @details	Reads the element pointed to by _out\_element_ from the buffer pointed to by _buffer_. DOES change the element!
 * @param	buffer Pointer to the circular buffer.
 * @param	out_element Pointer to the element to be written to.
 * @param	semaphores Pointer to a collection of semaphores used for synchronization.
 * @return	**CIRC_BUF_OK** on successful write, **CIRC_BUF_ERROR** if an error is encountered.
 */
int circ_buf_read(struct circ_buffer* buffer, struct circ_buffer_element* out_element, struct circ_buffer_semaphores* semaphores)
{
	if(sem_wait(semaphores->num_of_written_sem) < 0) return CIRC_BUF_ERROR;

	memmove((void*)out_element, (void*)&(buffer->elements[buffer->first_written_ind]), sizeof(struct circ_buffer_element));
	buffer->first_written_ind = ((buffer->first_written_ind) + 1) % CIRC_BUF_SIZE;

	if(sem_post(semaphores->num_of_free_sem) < 0) return CIRC_BUF_ERROR;
	
	return CIRC_BUF_OK;
}