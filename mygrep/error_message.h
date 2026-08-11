/**
 * @file error_message.h
 * @author Antonio Molina Gradischnig, 12421907
 * @date 27.10.2025
 *
 * @brief File containing error message macros.
 * @details This file contains macros for printing error messages. It contains
 * macros for printing an error message with perror, an error message with a
 * format string, and an error message with a string. Global variables: cmd
 */

#ifndef ERROR_MESSAGE_H
#define ERROR_MESSAGE_H
#include <stdio.h>
#include <stdlib.h>

extern char *cmd;
/**
 * @brief Prints an error message with perror.
 * @details This macro prints an error message with perror. It prints the
 * program name and the error message as using perror. The cmd variable must be
 * set before using this macro.
 */
#define eperror(MSG)                                                           \
  do {                                                                         \
    fprintf("%s: ", cmd);                                                      \
    perror(MSG);                                                               \
  } while (0)

/**
 * @brief Prints an error message with a format string.
 * @details This macro prints an error message with a format string. It prints
 * the program name and the error message passed as a format string. The cmd
 * variable must be set before using this macro.
 */
#define eprintf(MSG, ...)                                                      \
  do {                                                                         \
    fprintf(stderr, "%s: ", cmd);                                              \
    fprintf(stderr, MSG, __VA_ARGS__);                                         \
  } while (0)

/**
 * @brief Prints an error message with a string.
 * @details This macro prints an error message with a string. It prints the
 * program name and the error message passed as a string. The cmd variable must
 * be set before using this macro.
 */
#define eprint(MSG)                                                            \
  do {                                                                         \
    fprintf(stderr, "%s: %s", cmd, (MSG));                                     \
  } while (0)

#endif
