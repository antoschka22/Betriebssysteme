/**
 * @author Jonas Müller, 12223225
 * @date 2023-10-28
 *
 * @brief This header file contains the debugging macros. 
 * @details This header file defines two debug macros, one for printing a simple message and one for printing a message with the file, line and function name. The macro only prints if the -DDEBUG flag is set during compilation.
 */

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef DEBUGGING_H
#define DEBUGGING_H

#include <stdio.h>

#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

#define debug_print_extended(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

#endif
