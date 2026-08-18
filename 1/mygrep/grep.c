/**
 * @file grep.c
 * @author Antonio Molina Gradischnig, 12421907
 * @date 18.10.2025
 *
 * @brief This file contains the implementation of the function grep.
 * @details This file allows to grep a string from a file or stdin.
 */
#include "grep.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/**
 * Make the string lower case.
 * @brief Make the string lower case.
 * @details This function makes the string lower case.
 * @param string The string to make lower case.
 */
static void to_lower_case(char *string);
/**
 * Check if the string includes the search string.
 * @brief Check if the string includes the search string.
 * @details This function checks if the string includes the search string. If
 * ignore_case is true, the casing is ignored.
 * @param string The string to check.
 * @param search_string The search string.
 * @param ignore_case Ignore the casing.
 */

static bool includes(char *string, char *search_string, bool ignore_case);

int grep(GrepArgs_t args) {
  for (int i = 0; i < args.input_file_counter; ++i) {
    FILE *fp = args.input_files[i];
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) > 0) {
      if (includes(line, args.search_str, args.ignore_case)) {
        fprintf(args.output, "%s", line);
      }
    }
    free(line);
    line = NULL;
  }
  return 0;
}

static bool includes(char *string, char *search_string, bool ignore_case) {
  uint32_t string_len = strlen(string);
  char *string_copy = malloc(string_len + 1);

  if (string_copy == NULL) {
    fprintf(stderr, "grep.c|includes: Error when allocating string_copy\n");
    exit(EXIT_FAILURE);
  }

  strcpy(string_copy, string);
  uint32_t searchString_len = strlen(search_string);
  char *searchString_copy = malloc(searchString_len + 1);

  if (searchString_copy == NULL) {
    fprintf(stderr, "grep.c|includes: Error when allocating searchString_copy\n");
    free(string_copy);
    exit(EXIT_FAILURE);
  }

  strcpy(searchString_copy, search_string);
  if (ignore_case) {
    to_lower_case(searchString_copy);
    to_lower_case(string_copy);
  }
  bool result = false;
  if (strstr(string_copy, searchString_copy) != NULL) {
    result = true;
  }
  free(string_copy);
  string_copy = NULL;
  free(searchString_copy);
  searchString_copy = NULL;
  return result;
}
static void to_lower_case(char *string) {
  for (int i = 0; string[i]; ++i) {
    string[i] = tolower(string[i]);
  }
}
