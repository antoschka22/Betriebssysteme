/**
 * @author Antonio Molina Gradischnig, 12421907
 * @date 2025-10-28
 *
 * @brief This file contains the function for merging and printing the lines.
 * @details This file contains the function for merging and printing the lines.
 */
#include "printMerged.h"
#include <stdlib.h>

void printMerged(FILE *printTo, char **lines1, int numberLines1, char **lines2, int numberLines2)
{
    int index1 = 0;
    int index2 = 0;

    while (index1 < numberLines1 || index2 < numberLines2)
    {
        if (index2 >= numberLines2)
        {
            // just print lines1, lines2 is exhausted
            if (printf("%s", lines1[index1]) < 0)
            {
                perror("Error printing to stdout");
                exit(EXIT_FAILURE);
            }
            ++index1;
            continue;
        }

        if (index1 >= numberLines1)
        {
            // just print lines2, lines1 is exhausted.
            if (printf("%s", lines2[index2]) < 0)
            {
                perror("Error printing to stdout");
                exit(EXIT_FAILURE);
            }
            ++index2;
            continue;
        }

        // compare and print smaller one
        int compareIndex = 0;
        while (lines1[index1][compareIndex] == lines2[index2][compareIndex] && lines1[index1][compareIndex] && lines2[index2][compareIndex])
        {
            ++compareIndex;
        }

        // compareIndex is now index of the first different number
        if (lines1[index1][compareIndex] < lines2[index2][compareIndex])
        {
            if (printf("%s", lines1[index1]) < 0)
            {
                perror("Error printing to stdout");
                exit(EXIT_FAILURE);
            }
            ++index1;
        }
        else
        {
            if (printf("%s", lines2[index2]) < 0)
            {
                perror("Error printing to stdout");
                exit(EXIT_FAILURE);
            }
            ++index2;
        }
    }
}
