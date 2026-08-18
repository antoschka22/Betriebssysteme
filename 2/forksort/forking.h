/**
 * @author Antonio Molina Gradischnig, 12421907
 * @date 2025-10-28
 *
 * @brief This header file contains the forking functions.
 * @details This header file contains the functions for starting a child process and interacting with the pipes of the child processes.
 */
#ifndef FORKING_H
#define FORKING_H

#include <stdio.h>
#include <unistd.h>

/**
 * @brief Struct for saving data of a child process.
 * @details This struct saves the pid of the child process and the file descriptors of the pipes for stdin and stdout.
 */
struct processData
{
    pid_t pid;
    int pipeStdin[2];
    int pipeStdout[2];
};

/**
 * @brief Starts a child process and returns the data of the child process.
 * @details This function starts a child process running this program itself with links the stdin and stdout to the pipes in the returned processData struct.
 * @param argc The number of arguments
 * @param argv The arguments
 * @return The data of the child process
 */
struct processData startChild(int argc, char **argv);

/**
 * @brief Reads data from a file and saves it to an array.
 * @details This function reads data from a file and saves it to an array. The array is dynamically allocated and the number of lines read is saved in the numberOfLinesPointer. The initial size of the array can be specified with the initNumberOfLines parameter.
 * @param linesArrayPointer Pointer to the array where the lines should be saved
 * @param initNumberOfLines The initial size of the array
 * @param numberOfLinesPointer Pointer to the variable where the number of lines read should be saved
 * @param file The file to read from
 * @return void
 */
void readFile(char ***linesArrayPointer, int initNumberOfLines, int *numberOfLinesPointer, FILE *file);

#endif
