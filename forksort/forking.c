/**
 * @author Antonio Molina Gradischnig, 12421907
 * @date 2025-10-28
 *
 * @brief This module contains the forking functions.
 * @details This module contains the functions for starting a child process and interacting with the pipes of the child processes.
 */
#include "forking.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct processData startChild(int argc, char **argv)
{
    struct processData process;

    if (pipe(process.pipeStdout) != 0)
    {
        perror("Error with pipeStdout");
        exit(EXIT_FAILURE);
    }
    if (pipe(process.pipeStdin) != 0)
    {
        perror("Error with pipeStdin");
        exit(EXIT_FAILURE);
    }

    process.pid = fork();

    if (process.pid == 0)
    {
        if (close((process.pipeStdout)[0]) != 0) {
            perror("Error closing pipeStdout[0]");
            exit(EXIT_FAILURE);
        }
        if (close((process.pipeStdin)[1]) != 0) {
            perror("Error closing pipeStdin[1]");
            exit(EXIT_FAILURE);
        }

        if (dup2((process.pipeStdout)[1], STDOUT_FILENO) == -1) {
            perror("Error dup2 pipeStdout[1]");
            exit(EXIT_FAILURE);
        }
        if (dup2((process.pipeStdin)[0], STDIN_FILENO) == -1) {
            perror("Error dup2 pipeStdin[0]");
            exit(EXIT_FAILURE);
        }

        if (close((process.pipeStdout)[1]) != 0) {
            perror("Error closing pipeStdout[1]");
            exit(EXIT_FAILURE);
        }
        if (close((process.pipeStdin)[0]) != 0) {
            perror("Error closing pipeStdin[0]");
            exit(EXIT_FAILURE);
        }

        execlp(*argv, *argv, NULL);
        
        perror("program fork did not not start\n");
        exit(EXIT_FAILURE);
    }

    if (close(process.pipeStdout[1]) != 0) {
        perror("Error closing pipeStdout[1]");
        exit(EXIT_FAILURE);
    }
    if (close(process.pipeStdin[0]) != 0) {
        perror("Error closing pipeStdin[0]");
        exit(EXIT_FAILURE);
    }

    return process;
}

void readFile(char ***linesArrayPointer, int initNumberOfLines, int *numberOfLinesPointer, FILE *file)
{
    // variables for reading from file
    char *line = NULL;
    size_t buf = 0;
    ssize_t lenRead;

    // variables for saving read data
    int numberOfLines = 0;
    int linesArrCapacity = initNumberOfLines;
    char **lines = calloc(linesArrCapacity, sizeof(char *));

    if (lines == NULL)
    {
        perror("Error allocating memory!");
        exit(EXIT_FAILURE);
    }

    while ((lenRead = getline(&line, &buf, file)) != -1)
    {
        if (linesArrCapacity == numberOfLines)
        {
            linesArrCapacity *= 2;
            lines = realloc(lines, linesArrCapacity * sizeof(char *));
            if (lines == NULL)
            {
                perror("Error reallocating memory!");
                exit(EXIT_FAILURE);
            }
        }

        lines[numberOfLines] = line;
        ++numberOfLines;
        line = NULL;
    }
    
    free(line);
    line = NULL;

    *linesArrayPointer = lines;
    *numberOfLinesPointer = numberOfLines;
}
