/**
 * @file mygrep.c
 * @author Antonio Molina Gradischnig, 12421907
 * @date 18.10.2025
 *
 * @brief This file contains the main function.
 * @details Grep a string from a file or stdin.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "decode_argv.h"
#include "grep.h"
#include "error_message.h"

/**
 * Main function.
 * @brief Main function.
 * @details This function is the entry point of the program.
 * @param argc The argument counter.
 * @param argv The argument vector.
 * @return EXIT_SUCCESS if the program was successful, EXIT_FAILURE otherwise.
 */
int main(int argc, char **argv)
{
    cmd = *argv;
    GrepArgs_t args = decode_argv(argc, argv);
    
    if (grep(args) < 0)
    {
        fprintf(stderr, "Something went wrong");
    }

    cleanup_grep_args(&args);
    return EXIT_SUCCESS;
}
