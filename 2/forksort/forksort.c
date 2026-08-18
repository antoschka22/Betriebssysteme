/**
 * @author Antonio Molina Gradischnig, 12421907
 * @date 2025-10-28
 *
 * @brief This program reads lines from stdin, sorts them and prints them to stdout.
 * @details This program reads lines from stdin, sorts them and prints them to stdout using two child processes like the mergesort algorithm.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#include "debugging.h"
#include "printMerged.h"
#include "forking.h"

#define STD_ARR_SIZE 8 /* Minimum size of array for reading data from stdin */
#define USAGE_MESSAGE_FILE stdout /* The file to print the usage message to */

/**
 * Mandatory usage function
 * @brief Prints the usage of the program
 * @details This function prints the usage of the program
 * @param argv The arguments
 * @return void
 */
void usage(char **argv)
{
    if (fprintf(USAGE_MESSAGE_FILE, "Usage: %s\n", *argv) < 0)
    {
        perror("Error printing to stdout");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Cleans up the memory of the lines array.
 * @details This function cleans up the memory of the specified lines array.
 * @param lines The lines array
 * @param numberOfLines The number of lines
 * @return void
 */
void cleanup(char **lines, int numberOfLines)
{
    for (int i = 0; i < numberOfLines; i++)
    {
        free(lines[i]);
    };
    free(lines);
}

/**
 * @brief The main function of the program forksort.
 * @details This function reads lines from stdin, sorts them and prints them to stdout using two child processes like the mergesort algorithm.
 * @param argc The number of arguments
 * @param argv The arguments
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure
 */
int main(int argc, char **argv)
{
    // check if there are any arguments, if so print usage and exit
    if (argc != 1)
    {
        usage(argv);
        debug_print("%s\n", "There should be no arguments!");
        exit(EXIT_FAILURE);
    }

    char* line1 = NULL;
    size_t n = 0;
    int lenRead = 0;
    if ((lenRead = getline(&line1, &n, stdin)) == -1) {
        // got no data -> error
        free(line1);
        line1 = NULL;
        if (fprintf(stderr, "Got no data!\n") < 0) {
            perror("Error printing to stderr");
        }
        exit(EXIT_FAILURE);
    }
    char* line2 = NULL;
    if ((lenRead = getline(&line2, &n, stdin)) == -1) {
        // got one line -> base case, no splitting
        if (printf("%s", line1) < 0) {
            perror("Error printing to stdout");
            exit(EXIT_FAILURE);
        }
        free(line1);
        free(line2);
        line1 = NULL;
        line2 = NULL;
        debug_print("%s\n", "Got one line, returning!");
        exit(EXIT_SUCCESS);
    }

    // fork and write first two lines to children
    struct processData process1 = startChild(argc, argv);
    struct processData process2 = startChild(argc, argv);

    // open stdin pipes of children
    FILE *pipe1StdinFile = fdopen(process1.pipeStdin[1], "w");
    if (pipe1StdinFile == NULL) {
        perror("Error opening stdin pipe of pipe 1");
        exit(EXIT_FAILURE);
    }

    FILE *pipe2StdinFile = fdopen(process2.pipeStdin[1], "w");
    if (pipe2StdinFile == NULL) {
        perror("Error opening stdin pipe of pipe 2");
        exit(EXIT_FAILURE);
    }

    // write first two lines to children
    if (fprintf(pipe1StdinFile, "%s", line1) < 0) {
        perror("Error writing to stdin of pipe 1");
        exit(EXIT_FAILURE);
    }
    if (fprintf(pipe2StdinFile, "%s", line2) < 0) {
        perror("Error writing to stdin of pipe 2");
        exit(EXIT_FAILURE);
    }

    free(line1);
    free(line2);
    line1 = NULL;
    line2 = NULL;

    debug_print("%s\n", "done forking");

    // write incoming data to either the first or the second pipe
    char* currLine = NULL;
    int numberOfLines = 2;
    lenRead = 0;
    while ((lenRead = getline(&currLine, &n, stdin)) != -1)
    {
        if ((numberOfLines % 2) == 0) {
            if (fprintf(pipe1StdinFile, "%s", currLine) < 0) {
                perror("Error writing to stdin of pipe 1");
                exit(EXIT_FAILURE);
            }
        } else {
            if (fprintf(pipe2StdinFile, "%s", currLine) < 0) {
                perror("Error writing to stdin of pipe 2");
                exit(EXIT_FAILURE);
            }
        }
        ++numberOfLines;
        free(currLine);
        currLine = NULL;
    }
    free(currLine);
    currLine = NULL;
    
    debug_print("done writing %d lines\n", numberOfLines);

    // close files and pipes

    if (fclose(pipe1StdinFile) == EOF) {
        perror("Error closing stdin pipe of pipe 1");
        exit(EXIT_FAILURE);
    }
    if (fclose(pipe2StdinFile) == EOF) {
        perror("Error closing stdin pipe of pipe 2");
        exit(EXIT_FAILURE);
    }

    debug_print("%s\n", "pipes are closed");

    int succ = EXIT_FAILURE;
    // wait for the children to return and check their return values
    if (waitpid(process1.pid, &succ, 0) == -1 || succ != EXIT_SUCCESS)
    {
        perror("error on child 1");
        return EXIT_FAILURE;
    }

    debug_print("%s\n", "child 1 returned successfully");

    succ = EXIT_FAILURE;
    if (waitpid(process2.pid, &succ, 0) == -1 || succ != EXIT_SUCCESS)
    {
        perror("error on child 2");
        return EXIT_FAILURE;
    }

    debug_print("%s\n", "child 2 returned successfully");

    // read data from the two stdout pipes
    char **lines1;
    FILE *pipe1StdoutFile = fdopen(process1.pipeStdout[0], "r");
    int numberOfLines1;

    if (pipe1StdoutFile == NULL) {
        perror("Error opening stdout pipe of pipe 1");
        exit(EXIT_FAILURE);
    }

    readFile(&lines1, numberOfLines / 2, &numberOfLines1, pipe1StdoutFile);
    fclose(pipe1StdoutFile);

    debug_print("Read %d lines from child 1\n", numberOfLines1);

    char **lines2;
    FILE *pipe2StdoutFile = fdopen(process2.pipeStdout[0], "r");
    int numberOfLines2;

    if (pipe2StdoutFile == NULL) {
        perror("Error opening stdout pipe of pipe 2");
        exit(EXIT_FAILURE);
    }

    readFile(&lines2, numberOfLines - (numberOfLines / 2), &numberOfLines2, pipe2StdoutFile);
    
    if (fclose(pipe2StdoutFile) == EOF) {
        perror("Error closing stdout pipe of pipe 2");
        exit(EXIT_FAILURE);
    }

    if (numberOfLines != numberOfLines1 + numberOfLines2)
    {
        fprintf(stderr, "Error: number of lines read from children does not match number of lines written to children\n");
        exit(EXIT_FAILURE);
    }

    debug_print("Read %d lines from child 2\n", numberOfLines2);

    debug_print("start merging of %d and %d lines\n", numberOfLines1, numberOfLines2);

    // merge the two arrays and print the result to stdout
    printMerged(stdout, lines1, numberOfLines1, lines2, numberOfLines2);

    debug_print("%s\n", "done merging");

    // cleanup
    cleanup(lines1, numberOfLines1);
    cleanup(lines2, numberOfLines2);

    return EXIT_SUCCESS;
}
