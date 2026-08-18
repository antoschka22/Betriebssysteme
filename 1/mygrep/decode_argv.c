/**
 * @file decode_argv.c
 * @author Antonio Molina Gradischnig, 12421907
 * @date 18.10.2025
 *
 * @brief This file contains the implementation of the function decode_argv.
 * @details This file allows to decode the arguments of the grep program. If en
 * error occurs, the program exits with the corresponding error code.
 */

#include "decode_argv.h"
#include "error_message.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief This is the error code when no error occured.
 * @details This is the error code when no error occured.
 */
#define NO_ERROR 0
/**
 * @brief This is the error code for a usage error.
 * @details This is the error code for a usage error.
 */
#define USAGE_ERROR 1
/**
 * @brief This is the error code for a file read error.
 * @details This is the error code for a file read error.
 */
#define FILE_READ_ERROR 2
/**
 * @brief This is the error code for a file write error.
 * @details This is the error code for a file write error.
 */
#define FILE_WRITE_ERROR 3
/**
 * @brief This is the error code for an unexpected error.
 * @details This is the error code for an unexpected error.
 */
#define UNEXPECTED_ERROR (-1)

/**
 * Mandatory usage fuction.
 * @brief Prints the usage of the grep program to stderr.
 * @details This function prints the usage of the grep program to stderr and
 * exits with EXIT_FAILURE.
 * @param argv The argv variable of the main function.
 */
static void print_usage(char **argv) {
  printf("%s [-i] [-o outfile] keyword [file...]\n", *argv);
  exit(EXIT_FAILURE);
}

/**
 * Read the outfile name from the optarg.
 * @brief Reads the outfile name from the optarg.
 * @details This function reads the outfile name from the optarg and stores it
 * in the outfile_name variable.
 * @param outfile_name The variable to store the outfile name in.
 * @return NO_ERROR if no error occured, USAGE_ERROR if the outfile_name is
 * already set.
 */
static int read_outfile_name(char **outfile_name) {
  if (*outfile_name != NULL) {
    return USAGE_ERROR;
  }
  *outfile_name = optarg;
  return NO_ERROR;
}

/**
 * Parse the options.
 * @brief Parses the options.
 * @details This function parses the options and stores the results in the args
 * variable.
 * @param outfile_name The variable to store the outfile name in.
 * @param args The variable to store the arguments in.
 * @param argc The argc variable of the main function.
 * @param argv The argv variable of the main function.
 * @return NO_ERROR if no error occured, USAGE_ERROR if an invalid option was
 * given.
 */
static int parse_options(char **outfile_name, GrepArgs_t *args, int argc,
                         char **argv) {
  int option;
  while ((option = getopt(argc, argv, "io:")) > 0) {
    switch (option) {
    case 'i':
      if (args->ignore_case) {
        return USAGE_ERROR;
      }
      args->ignore_case = true;
      break;
    case 'o':
      read_outfile_name(outfile_name);
      break;
    default:
      return USAGE_ERROR;
    }
  }
  return NO_ERROR;
}

/**
 * Parse the input files.
 * @brief Parses the input files.
 * @details This function parses the input files and stores the resulting FILE*
 * in the args variable.
 * @param args The variable to store the arguments in.
 * @param input_files The input file names. Reads args->input_file_counter
 * filenames.
 * @return NO_ERROR if no error occured, FILE_READ_ERROR if a file could not be
 * read. UNEXPECTED_ERROR if an unexpected error occured.
 */
static int parse_input_files(GrepArgs_t *args, char **input_files) {
  if (args->input_file_counter > 0) {
    args->input_files = calloc(sizeof(FILE *), args->input_file_counter);
    if (args->input_files == NULL) {
      return UNEXPECTED_ERROR;
    }
    memset(args->input_files, 0, sizeof(FILE *) * args->input_file_counter);
    for (int i = 0; i < args->input_file_counter; ++i) {
      if ((args->input_files[i] = fopen(input_files[i], "r")) == NULL) {
        eprintf("Could not read file %s\nDetails: %s\n", input_files[i],
                strerror(errno));
        return FILE_READ_ERROR;
      }
    }
  } else {
    args->input_files = calloc(sizeof(FILE *), 1);
    if (args->input_files == NULL) {
      return UNEXPECTED_ERROR;
    }
    *(args->input_files) = stdin;
    args->input_file_counter = 1;
  }
  return NO_ERROR;
}

/**
 * Check if the pattern is given.
 * @brief Checks if the pattern is given.
 * @details This function checks if the pattern is given.
 * @param argc The argc variable of the main function.
 * @return NO_ERROR if no error occured, USAGE_ERROR if the pattern is not
 * given.
 */
static int check_pattern(int argc) {
  if (argc - optind < 1) {
    return USAGE_ERROR;
  }
  return NO_ERROR;
}

/**
 * Open the outfile.
 * @brief Opens the outfile.
 * @details This function opens the outfile if the outfile_name != NULL and
 * returns an error code.
 * @param args The variable to store the arguments in.
 * @param outfile_name The outfile name. (Can be NULL)
 * @return NO_ERROR if no error occured, FILE_WRITE_ERROR if the outfile could
 * not be opened.
 */
static int open_outfile(GrepArgs_t *args, char *outfile_name) {
  FILE *file = NULL;
  if (outfile_name != NULL) {
    if ((file = fopen(outfile_name, "w")) == NULL) {
      eprintf("Could not write file %s\nDetails: %s\n", outfile_name,
              strerror(errno));
      return FILE_WRITE_ERROR;
    }
    args->output = file;
  }
  return NO_ERROR;
}

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
GrepArgs_t decode_argv(int argc, char **argv) {
  int error;
  GrepArgs_t args = {
      .output = stdout, .ignore_case = false, .input_files = NULL};
  char *outfile_name = NULL;

  if ((error = parse_options(&outfile_name, &args, argc, argv)) != NO_ERROR) {
    goto cleanup_error_handling;
  }

  if ((error = check_pattern(argc)) != NO_ERROR) {
    goto cleanup_error_handling;
  }

  args.search_str = argv[optind];
  ++optind;

  if ((error = open_outfile(&args, outfile_name)) != NO_ERROR) {
    goto cleanup_error_handling;
  }

  char **input_files = argv + optind;
  args.input_file_counter = argc - optind;
  if ((error = parse_input_files(&args, input_files)) != NO_ERROR) {
    goto cleanup_error_handling;
  }

cleanup_error_handling:
  if (error != NO_ERROR) {
    cleanup_grep_args(&args);
  }
  switch (error) // error handling
  {
  case USAGE_ERROR:
    print_usage(argv);
    break;
  case FILE_READ_ERROR:
  case FILE_WRITE_ERROR:
    exit(EXIT_FAILURE);
    break;
  case UNEXPECTED_ERROR:
    eprintf("ERROR: %s", strerror(errno));
    exit(EXIT_FAILURE);
    break;
  }

  return args;
}

/**
 * Cleanup the grep arguments.
 * @brief Cleans up the grep arguments.
 * @details This function cleans up the grep arguments. It closes all open files
 * and frees all allocated memory.
 * @param args The grep arguments to clean up.
 */
void cleanup_grep_args(GrepArgs_t *args) {
  if ((args->output != stdout) && (args->output != NULL)) {
    fclose(args->output);
  }
  for (int i = 0; i < args->input_file_counter; ++i) {
    if (args->input_files[i] != NULL && args->input_files[i] != stdin) {
      fclose(args->input_files[i]);
    }
  }
  free(args->input_files);
  args->input_files = NULL;
}
