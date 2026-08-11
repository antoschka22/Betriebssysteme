/**
 * @file	printdebug.h
 * @author	Antonio Molina Gradischnig, 12421907
 * @brief	Contains text output functions.
 * @details	Contains an error-printing function and debug-printing macro.
 * @date	13-11-2025
 */
#pragma once

#include "commons.h"
#include <stdio.h>

/// For debug printing purposes. Works only if 'DEBUG' symbol is defined.
#ifdef DEBUG
#define print_debug(ctx, format, ...) (void)fprintf(stderr, "[%-12s@%-6d | %s:%-3d] " format "\n", ctx.progname, ctx.pid,  __func__, __LINE__, ##__VA_ARGS__)
#else
#define print_debug(ctx, format, ...) // Empty to avoid errors
#endif

void print_error(struct program_ctx* ctx, const char* msg, int errnum);