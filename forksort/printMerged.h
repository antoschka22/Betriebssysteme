/**
 * @author Antonio Molina Gradischnig, 12421907
 * @date 2025-10-28
 *
 * @brief This header file contains the function for merging and printing the lines.
 * @details This header file contains the function for merging and printing the lines.
 */
#ifndef PRINT_MERGED_H
#define PRINT_MERGED_H
#include <stdio.h>

/**
 * @brief Prints the merged lines to the specified file
 * @details This function merges the lines from the two arrays and prints them to the specified file.
 * @param printTo The file to print to
 * @param lines1 The first array of lines
 * @param numberLines1 The number of lines in the first array
 * @param lines2 The second array of lines
 * @param numberLines2 The number of lines in the second array
 * @return void
*/
void printMerged(FILE* printTo, char** lines1, int numberLines1, char** lines2, int numberLines2); 

#endif
