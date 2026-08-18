/**
 * @file decode_argv.h
 * @author Antonio Molina Gradischnig, 12421907
 * @date 18.10.2025
 *
 * @brief This file contains the declaration of the function decode_argv.
 * @details This file allows to decode the arguments of the grep program. If en
 * error occurs, the program exits with the corresponding error code.
 */
#ifndef DECODE_ARGV_H
#define DECODE_ARGV_H

#include "grep.h"
#include <stdint.h>
#include <stdlib.h>

/**
 * Decode the arguments.
 * @brief Decodes the arguments.
 * @details This function decodes the arguments and returns the parsed arguments
 * as a GrepArgs_t struct. If an error occurs the program exits with the
 * corresponding error code.
 * @param argc The argc variable of the main function.
 * @param argv The argv variable of the main function.
 * @return The parsed arguments as a GrepArgs_t struct.
 */
GrepArgs_t decode_argv(int argc, char **argv);

/**
 * Cleanup the grep arguments.
 * @brief Cleans up the grep arguments.
 * @details This function cleans up the grep arguments. It closes all open files
 * and frees all allocated memory.
 * @param args The grep arguments to clean up.
 */
void cleanup_grep_args(GrepArgs_t *args);

#endif
