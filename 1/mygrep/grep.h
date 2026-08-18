/**
 * @file grep.h
 * @author Antonio Molina Gradischnig, 12421907
 * @date 18.10.2025
 *
 * @brief This file contains the declaration of the function grep.
 * @details This file allows to grep a string from a file or stdin.
 */
#ifndef GREP_H
#define GREP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/**
 * Grep arguments.
 * @brief Grep arguments.
 * @details This struct contains the arguments for the grep function.
 */
typedef struct {
  FILE *output;
  bool ignore_case;
  char *search_str;
  uint32_t input_file_counter;
  FILE **input_files;
} GrepArgs_t;

/**
 * Grep function.
 * @brief Search the search string in the input files.
 * @details This function searches the search string in the input files and
 * writes the results to the output file.
 */
int grep(GrepArgs_t args);

#endif
