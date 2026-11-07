/**
 * @file quadtree.c
 * @author Antonio Molina Gradischnig (12421907)
 * @brief Recursive 2D spatial point query using a quadtree process hierarchy.
 * @details Reads 2D points from stdin, partitions the search space recursively
 *          into four quadrants via fork(2), execlp(3), and unnamed pipes (pipe(2)),
 *          and outputs all points within a specified maximum Euclidean distance
 *          from a target point.
 * @date 2026-11-06
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/** Number of quadrants / child processes per split */
#define NUM_CHILDREN (4)

/** Buffer size for formatting float arguments for execlp */
#define FLOAT_STR_LEN (64)

/**
 * @brief Enumeration for pipe file descriptor indices.
 */
typedef enum {
    PIPE_READ = 0,
    PIPE_WRITE = 1
} pipe_end_t;

/**
 * @brief Enumeration for the four quadrants in required merge order.
 */
typedef enum {
    QUAD_BOTTOM_LEFT = 0,
    QUAD_BOTTOM_RIGHT = 1,
    QUAD_TOP_LEFT = 2,
    QUAD_TOP_RIGHT = 3
} quadrant_t;

/**
 * @brief Represents a 2D point with x and y coordinates.
 */
typedef struct {
    float x;
    float y;
} point_t;

/**
 * @brief Represents the 2D bounding box limits of a quadrant.
 */
typedef struct {
    float min_x;
    float max_x;
    float min_y;
    float max_y;
} limits_t;

/**
 * @brief Manages IPC resources and PID for a single child process.
 */
typedef struct {
    pid_t pid;
    int pipe_req[2];  /**< Parent writes (stdin for child), Child reads */
    int pipe_resp[2]; /**< Child writes (stdout for child), Parent reads */
    FILE *req_stream; /**< Buffered stream wrapping pipe_req[PIPE_WRITE] */
    FILE *resp_stream;/**< Buffered stream wrapping pipe_resp[PIPE_READ] */
} child_t;

/** Global program name for error reporting (Guideline 11) */
static const char *program_name = "quadtree";

/* --- Function Prototypes --- */

static void usage(void);
static void error_exit(const char *msg, child_t children[NUM_CHILDREN],
                       char *buf1, char *buf2);
static void init_children(child_t children[NUM_CHILDREN]);
static void cleanup_all(child_t children[NUM_CHILDREN], char *buf1, char *buf2);
static int safe_close(int *fd);
static int safe_fclose(FILE **stream);

static int parse_float_strict(const char *str, float *out_val);
static int parse_point(const char *str, point_t *point);
static float point_dist(const point_t *a, const point_t *b);
static void ensure_newline_ stripped_cmp(const char *a, const char *b, bool *equal);

static void limits_pivot(const limits_t *limits, point_t *pivot);
static void limits_split(const limits_t *limits, limits_t child_limits[NUM_CHILDREN]);
static quadrant_t get_quadrant(const point_t *p, const point_t *pivot);

static int read_line(char **line, size_t *cap, FILE *stream,
                     child_t children[NUM_CHILDREN], char *other_buf);
static void write_point_line(FILE *stream, const char *line,
                             child_t children[NUM_CHILDREN], char *b1, char *b2);

static void spawn_child(int idx, child_t children[NUM_CHILDREN],
                        const char *goal_str, const char *dist_str,
                        const limits_t *limits, char *b1, char *b2);

/**
 * @brief Program entry point.
 * @details Uses global variable program_name.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char *argv[]) {
    if (argc > 0 && argv[0] != NULL) {
        program_name = argv[0];
    }

    /* Optional getopt check to satisfy UNIX conventions (Guideline 5) */
    int opt;
    /* Passing "+" stops at non-option arguments; ":" suppresses getopt error msg */
    while ((opt = getopt(argc, argv, "+")) != -1) {
        switch (opt) {
            default:
                usage();
        }
    }

    /* Note: Negative numbers look like options to getopt if not careful,
     * so we solely validate total argument count as required by synopsis. */
    if (argc != 7) {
        usage();
    }

    child_t children[NUM_CHILDREN];
    init_children(children);

    /* Parse arguments using strtof(3) */
    point_t goal;
    if (parse_point(argv[1], &goal) != 0) {
        error_exit("Invalid goal point format (expected <x>,<y>)", children, NULL, NULL);
    }

    float max_dist = 0.0f;
    if (parse_float_strict(argv[2], &max_dist) != 0 || max_dist < 0.0f) {
        error_exit("Invalid or negative distance", children, NULL, NULL);
    }

    limits_t limits;
    if (parse_float_strict(argv[3], &limits.min_x) != 0 ||
        parse_float_strict(argv[4], &limits.max_x) != 0 ||
        parse_float_strict(argv[5], &limits.min_y) != 0 ||
        parse_float_strict(argv[6], &limits.max_y) != 0) {
        error_exit("Invalid bounding box limits", children, NULL, NULL);
    }

    if (limits.min_x > limits.max_x || limits.min_y > limits.max_y) {
        error_exit("min_x/min_y must not exceed max_x/max_y", children, NULL, NULL);
    }

    /* Read first line from stdin */
    char *first_line = NULL;
    size_t first_cap = 0;
    if (read_line(&first_line, &first_cap, stdin, children, NULL) == -1) {
        /* Empty input -> terminate with EXIT_SUCCESS */
        cleanup_all(children, first_line, NULL);
        return EXIT_SUCCESS;
    }

    /* Validate first_line is a valid point immediately */
    point_t first_point;
    if (parse_point(first_line, &first_point) != 0) {
        error_exit("Invalid point on stdin", children, first_line, NULL);
    }

    /* Read subsequent lines, ignoring all lines equal to the first line */
    char *second_line = NULL;
    size_t second_cap = 0;
    bool found_distinct_line = false;

    while (read_line(&second_line, &second_cap, stdin, children, first_line) == 0) {
        /* Validate syntax of every line read */
        point_t tmp_pt;
        if (parse_point(second_line, &tmp_pt) != 0) {
            error_exit("Invalid point on stdin", children, first_line, second_line);
        }

        bool is_equal = false;
        ensure_newline_stripped_cmp(first_line, second_line, &is_equal);
        if (!is_equal) {
            found_distinct_line = true;
            break;
        }
    }

    /* Base case: only 1 unique line in input (or all lines identical to first) */
    if (!found_distinct_line) {
        float dist = point_dist(&goal, &first_point);
        if (dist <= max_dist) {
            fprintf(stdout, "%.3f: %s", dist, first_line);
            if (first_line[strlen(first_line) - 1] != '\n') {
                fprintf(stdout, "\n");
            }
        }
        cleanup_all(children, first_line, second_line);
        return EXIT_SUCCESS;
    }

    /* Recursive step: n > 1 distinct lines. Open pipes & spawn 4 children */
    limits_t child_limits[NUM_CHILDREN];
    limits_split(&limits, child_limits);

    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(children[i].pipe_req) == -1 || pipe(children[i].pipe_resp) == -1) {
            error_exit("pipe(2) failed", children, first_line, second_line);
        }
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        spawn_child(i, children, argv[1], argv[2], &child_limits[i],
                    first_line, second_line);
    }

    /* Define pipe orientation in parent: close child-side pipe ends */
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (safe_close(&children[i].pipe_req[PIPE_READ]) == -1 ||
            safe_close(&children[i].pipe_resp[PIPE_WRITE]) == -1) {
            error_exit("close(2) of unused pipe ends failed in parent",
                       children, first_line, second_line);
        }

        /* Wrap parent write ends in FILE* streams (Guideline 29) */
        children[i].req_stream = fdopen(children[i].pipe_req[PIPE_WRITE], "w");
        if (children[i].req_stream == NULL) {
            error_exit("fdopen(3) failed for request pipe",
                       children, first_line, second_line);
        }
        /* Ownership of FD transferred to FILE* stream */
        children[i].pipe_req[PIPE_WRITE] = -1;
    }

    /* Distribute points to respective quadrant children */
    point_t pivot;
    limits_pivot(&limits, &pivot);

    /* Write first_line */
    quadrant_t q1 = get_quadrant(&first_point, &pivot);
    write_point_line(children[q1].req_stream, first_line, children, first_line, second_line);

    /* Write second_line */
    point_t second_point;
    (void)parse_point(second_line, &second_point);
    quadrant_t q2 = get_quadrant(&second_point, &pivot);
    write_point_line(children[q2].req_stream, second_line, children, first_line, second_line);

    /* Read and distribute remaining lines from stdin, ignoring lines equal to first_line */
    while (read_line(&second_line, &second_cap, stdin, children, first_line) == 0) {
        point_t cur_point;
        if (parse_point(second_line, &cur_point) != 0) {
            error_exit("Invalid point on stdin", children, first_line, second_line);
        }

        bool is_equal = false;
        ensure_newline_stripped_cmp(first_line, second_line, &is_equal);
        if (is_equal) {
            continue; /* Step 3: Ignore all lines equal to the first line */
        }

        quadrant_t q = get_quadrant(&cur_point, &pivot);
        write_point_line(children[q].req_stream, second_line, children, first_line, second_line);
    }

    /* Close all write streams so children receive EOF on their stdin */
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (safe_fclose(&children[i].req_stream) == -1) {
            error_exit("fclose(3) failed on child request stream",
                       children, first_line, second_line);
        }
    }

    /* Merge output from children in order: BL, BR, TL, TR (before or after wait)
     * Reading responses BEFORE waiting prevents pipe buffer deadlocks on large inputs! */
    for (int i = 0; i < NUM_CHILDREN; i++) {
        children[i].resp_stream = fdopen(children[i].pipe_resp[PIPE_READ], "r");
        if (children[i].resp_stream == NULL) {
            error_exit("fdopen(3) failed for response pipe",
                       children, first_line, second_line);
        }
        children[i].pipe_resp[PIPE_READ] = -1;

        while (read_line(&second_line, &second_cap, children[i].resp_stream,
                         children, first_line) == 0) {
            if (fputs(second_line, stdout) == EOF) {
                error_exit("fputs(3) to stdout failed",
                           children, first_line, second_line);
            }
        }

        if (safe_fclose(&children[i].resp_stream) == -1) {
            error_exit("fclose(3) failed on child response stream",
                       children, first_line, second_line);
        }
    }

    /* Wait for all children and check their exit status */
    bool child_failed = false;
    for (int i = 0; i < NUM_CHILDREN; i++) {
        int status = 0;
        pid_t wpid;
        while ((wpid = waitpid(children[i].pid, &status, 0)) == -1) {
            if (errno == EINTR) {
                continue; /* Retry if interrupted by signal */
            }
            error_exit("waitpid(2) failed", children, first_line, second_line);
        }
        children[i].pid = -1;

        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
            child_failed = true;
        }
    }

    if (child_failed) {
        error_exit("One or more child processes terminated with failure",
                   children, first_line, second_line);
    }

    cleanup_all(children, first_line, second_line);
    return EXIT_SUCCESS;
}

/**
 * @brief Prints usage message to stdout (per Testing section) and exits with EXIT_FAILURE.
 * @details Uses global variable program_name.
 */
static void usage(void) {
    fprintf(stdout, "Usage: %s <goal> <distance> <min_x> <max_x> <min_y> <max_y>\n",
            program_name);
    exit(EXIT_FAILURE);
}

/**
 * @brief Cleans up open resources, prints an error message to stderr, and exits.
 * @details Uses global variable program_name.
 * @param msg Error description.
 * @param children Array of child_t structs to clean up.
 * @param buf1 First dynamic buffer to free (may be NULL).
 * @param buf2 Second dynamic buffer to free (may be NULL).
 */
static void error_exit(const char *msg, child_t children[NUM_CHILDREN],
                       char *buf1, char *buf2) {
    if (errno != 0) {
        fprintf(stderr, "[%s] Error: %s (%s)\n", program_name, msg, strerror(errno));
    } else {
        fprintf(stderr, "[%s] Error: %s\n", program_name, msg);
    }

    cleanup_all(children, buf1, buf2);

    /* Reap any already-spawned children to avoid zombies */
    if (children != NULL) {
        for (int i = 0; i < NUM_CHILDREN; i++) {
            if (children[i].pid > 0) {
                while (waitpid(children[i].pid, NULL, 0) == -1 && errno == EINTR) {
                    /* retry on EINTR */
                }
            }
        }
    }

    exit(EXIT_FAILURE);
}

/**
 * @brief Initializes child_t array with safe sentinel values (-1 and NULL).
 * @param children Array of child_t structures.
 */
static void init_children(child_t children[NUM_CHILDREN]) {
    for (int i = 0; i < NUM_CHILDREN; i++) {
        children[i].pid = -1;
        children[i].pipe_req[PIPE_READ] = -1;
        children[i].pipe_req[PIPE_WRITE] = -1;
        children[i].pipe_resp[PIPE_READ] = -1;
        children[i].pipe_resp[PIPE_WRITE] = -1;
        children[i].req_stream = NULL;
        children[i].resp_stream = NULL;
    }
}

/**
 * @brief Safely closes a file descriptor if it is valid (>= 0) and sets it to -1.
 * @param fd Pointer to the file descriptor.
 * @return 0 on success or if already closed, -1 on error.
 */
static int safe_close(int *fd) {
    if (fd != NULL && *fd >= 0) {
        int ret = close(*fd);
        *fd = -1;
        return ret;
    }
    return 0;
}

/**
 * @brief Safely closes a FILE stream if it is non-NULL and sets it to NULL.
 * @param stream Pointer to the FILE* variable.
 * @return 0 on success or if already NULL, -1 on error.
 */
static int safe_fclose(FILE **stream) {
    if (stream != NULL && *stream != NULL) {
        int ret = fclose(*stream);
        *stream = NULL;
        return (ret == 0) ? 0 : -1;
    }
    return 0;
}

/**
 * @brief Releases all open file descriptors, FILE streams, and dynamic buffers.
 * @param children Array of child_t structures.
 * @param buf1 First dynamic string buffer.
 * @param buf2 Second dynamic string buffer.
 */
static void cleanup_all(child_t children[NUM_CHILDREN], char *buf1, char *buf2) {
    if (children != NULL) {
        for (int i = 0; i < NUM_CHILDREN; i++) {
            (void)safe_fclose(&children[i].req_stream);
            (void)safe_fclose(&children[i].resp_stream);
            (void)safe_close(&children[i].pipe_req[PIPE_READ]);
            (void)safe_close(&children[i].pipe_req[PIPE_WRITE]);
            (void)safe_close(&children[i].pipe_resp[PIPE_READ]);
            (void)safe_close(&children[i].pipe_resp[PIPE_WRITE]);
        }
    }
    free(buf1);
    free(buf2);
}

/**
 * @brief Strictly parses a floating-point string using strtof(3).
 * @details Checks for empty string, no conversion (endptr == str), overflow/underflow
 *          (errno == ERANGE), NaN/Inf, and trailing non-null characters.
 * @param str Input string to parse.
 * @param out_val Pointer where parsed float is stored.
 * @return 0 on success, -1 on error.
 */
static int parse_float_strict(const char *str, float *out_val) {
    if (str == NULL || *str == '\0') {
        return -1;
    }

    char *endptr = NULL;
    errno = 0;
    float val = strtof(str, &endptr);

    if (errno == ERANGE || endptr == str || *endptr != '\0') {
        return -1;
    }
    if (isnan(val) || isinf(val)) {
        return -1;
    }

    *out_val = val;
    return 0;
}

/**
 * @brief Parses a 2D point formatted as "<x>,<y>" (with optional trailing newline).
 * @param str Input string.
 * @param point Output point_t structure.
 * @return 0 on success, -1 on failure.
 */
static int parse_point(const char *str, point_t *point) {
    if (str == NULL || *str == '\0') {
        return -1;
    }

    char *endptr_x = NULL;
    errno = 0;
    float x = strtof(str, &endptr_x);
    if (errno == ERANGE || endptr_x == str || *endptr_x != ',') {
        return -1;
    }

    const char *y_start = endptr_x + 1;
    /* Reject leading whitespace or empty string after comma */
    if (*y_start == '\0' || *y_start == '\n' || isspace((unsigned char)*y_start)) {
        return -1;
    }

    char *endptr_y = NULL;
    errno = 0;
    float y = strtof(y_start, &endptr_y);
    if (errno == ERANGE || endptr_y == y_start) {
        return -1;
    }

    /* Allow only '\0', '\n', or '\r\n' at the end of the y coordinate */
    if (*endptr_y == '\r') {
        endptr_y++;
    }
    if (*endptr_y == '\n') {
        endptr_y++;
    }
    if (*endptr_y != '\0') {
        return -1;
    }

    if (isnan(x) || isinf(x) || isnan(y) || isinf(y)) {
        return -1;
    }

    point->x = x;
    point->y = y;
    return 0;
}

/**
 * @brief Calculates Euclidean distance between two points using powf(3) and sqrtf(3).
 * @param a First point.
 * @param b Second point.
 * @return Distance between a and b.
 */
static float point_dist(const point_t *a, const point_t *b) {
    return sqrtf(powf(a->x - b->x, 2.0f) + powf(a->y - b->y, 2.0f));
}

/**
 * @brief Compares two lines for equality, ignoring trailing newline differences.
 * @param a First line string.
 * @param b Second line string.
 * @param equal Output boolean set to true if lines are identical.
 */
static void ensure_newline_stripped_cmp(const char *a, const char *b, bool *equal) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    while (len_a > 0 && (a[len_a - 1] == '\n' || a[len_a - 1] == '\r')) {
        len_a--;
    }
    while (len_b > 0 && (b[len_b - 1] == '\n' || b[len_b - 1] == '\r')) {
        len_b--;
    }

    if (len_a != len_b) {
        *equal = false;
        return;
    }
    *equal = (strncmp(a, b, len_a) == 0);
}

/**
 * @brief Computes the geometric center (pivot) of the given 2D limits.
 * @param limits Bounding box limits.
 * @param pivot Output center point.
 */
static void limits_pivot(const limits_t *limits, point_t *pivot) {
    pivot->x = (limits->min_x + limits->max_x) / 2.0f;
    pivot->y = (limits->min_y + limits->max_y) / 2.0f;
}

/**
 * @brief Splits bounding box limits into 4 quadrants: BL, BR, TL, TR.
 * @param limits Parent bounding box limits.
 * @param child_limits Output array of 4 quadrant limits.
 */
static void limits_split(const limits_t *limits, limits_t child_limits[NUM_CHILDREN]) {
    point_t pivot;
    limits_pivot(limits, &pivot);

    /* Bottom-Left */
    child_limits[QUAD_BOTTOM_LEFT].min_x = limits->min_x;
    child_limits[QUAD_BOTTOM_LEFT].max_x = pivot.x;
    child_limits[QUAD_BOTTOM_LEFT].min_y = limits->min_y;
    child_limits[QUAD_BOTTOM_LEFT].max_y = pivot.y;

    /* Bottom-Right */
    child_limits[QUAD_BOTTOM_RIGHT].min_x = pivot.x;
    child_limits[QUAD_BOTTOM_RIGHT].max_x = limits->max_x;
    child_limits[QUAD_BOTTOM_RIGHT].min_y = limits->min_y;
    child_limits[QUAD_BOTTOM_RIGHT].max_y = pivot.y;

    /* Top-Left */
    child_limits[QUAD_TOP_LEFT].min_x = limits->min_x;
    child_limits[QUAD_TOP_LEFT].max_x = pivot.x;
    child_limits[QUAD_TOP_LEFT].min_y = pivot.y;
    child_limits[QUAD_TOP_LEFT].max_y = limits->max_y;

    /* Top-Right */
    child_limits[QUAD_TOP_RIGHT].min_x = pivot.x;
    child_limits[QUAD_TOP_RIGHT].max_x = limits->max_x;
    child_limits[QUAD_TOP_RIGHT].min_y = pivot.y;
    child_limits[QUAD_TOP_RIGHT].max_y = limits->max_y;
}

/**
 * @brief Determines which quadrant a point belongs to relative to the pivot.
 * @param p Point to classify.
 * @param pivot Center pivot point.
 * @return Corresponding quadrant_t enum value.
 */
static quadrant_t get_quadrant(const point_t *p, const point_t *pivot) {
    int idx = 0;
    if (p->x > pivot->x) {
        idx += 1;
    }
    if (p->y > pivot->y) {
        idx += 2;
    }
    return (quadrant_t)idx;
}

/**
 * @brief Reads a line from stream using getline(3), checking for read errors.
 * @param line Pointer to line buffer.
 * @param cap Pointer to buffer capacity.
 * @param stream Input FILE* stream.
 * @param children Array of child_t for cleanup on fatal error.
 * @param other_buf Secondary buffer to free on fatal error.
 * @return 0 on success, -1 on clean EOF. Exits on error.
 */
static int read_line(char **line, size_t *cap, FILE *stream,
                     child_t children[NUM_CHILDREN], char *other_buf) {
    errno = 0;
    ssize_t nread = getline(line, cap, stream);
    if (nread == -1) {
        if (ferror(stream) || errno == EINVAL || errno == ENOMEM) {
            error_exit("getline(3) failed while reading stream",
                       children, *line, other_buf);
        }
        return -1; /* Clean EOF */
    }
    return 0;
}

/**
 * @brief Writes a point line to a child's request stream, ensuring a trailing newline.
 * @param stream Target FILE* stream.
 * @param line String to write.
 * @param children Array of child_t for error cleanup.
 * @param b1 Buffer 1 for error cleanup.
 * @param b2 Buffer 2 for error cleanup.
 */
static void write_point_line(FILE *stream, const char *line,
                             child_t children[NUM_CHILDREN], char *b1, char *b2) {
    if (fputs(line, stream) == EOF) {
        error_exit("fputs(3) to child pipe failed", children, b1, b2);
    }
    size_t len = strlen(line);
    if (len == 0 || line[len - 1] != '\n') {
        if (fputc('\n', stream) == EOF) {
            error_exit("fputc(3) to child pipe failed", children, b1, b2);
        }
    }
}

/**
 * @brief Forks and executes a child process for quadrant idx.
 * @details Uses global variable program_name. Redirects stdin and stdout via dup2(2)
 *          and closes all unneeded pipe file descriptors before calling execlp(3).
 * @param idx Index of child (0..3).
 * @param children Array of all children (needed so child can close all open pipe FDs).
 * @param goal_str Goal argument string.
 * @param dist_str Distance argument string.
 * @param limits Quadrant limits for this child.
 * @param b1 Dynamic buffer 1 to free in child on error/before exec.
 * @param b2 Dynamic buffer 2 to free in child on error/before exec.
 */
static void spawn_child(int idx, child_t children[NUM_CHILDREN],
                        const char *goal_str, const char *dist_str,
                        const limits_t *limits, char *b1, char *b2) {
    pid_t pid = fork();

    switch (pid) {
        case -1:
            error_exit("fork(2) failed", children, b1, b2);
            break;

        case 0: /* Child process */
            /* Redirect stdin to read end of child's request pipe */
            if (dup2(children[idx].pipe_req[PIPE_READ], STDIN_FILENO) == -1) {
                error_exit("dup2(2) for stdin failed in child", children, b1, b2);
            }
            /* Redirect stdout to write end of child's response pipe */
            if (dup2(children[idx].pipe_resp[PIPE_WRITE], STDOUT_FILENO) == -1) {
                error_exit("dup2(2) for stdout failed in child", children, b1, b2);
            }

            /* Close ALL pipe FDs inherited from parent (including the duplicated ones) */
            for (int i = 0; i < NUM_CHILDREN; i++) {
                if (safe_close(&children[i].pipe_req[PIPE_READ]) == -1 ||
                    safe_close(&children[i].pipe_req[PIPE_WRITE]) == -1 ||
                    safe_close(&children[i].pipe_resp[PIPE_READ]) == -1 ||
                    safe_close(&children[i].pipe_resp[PIPE_WRITE]) == -1) {
                    error_exit("close(2) failed in child", children, b1, b2);
                }
            }

            /* Free inherited dynamic buffers prior to exec (clean Valgrind state) */
            free(b1);
            free(b2);

            char min_x_s[FLOAT_STR_LEN];
            char max_x_s[FLOAT_STR_LEN];
            char min_y_s[FLOAT_STR_LEN];
            char max_y_s[FLOAT_STR_LEN];

            (void)snprintf(min_x_s, sizeof(min_x_s), "%f", limits->min_x);
            (void)snprintf(max_x_s, sizeof(max_x_s), "%f", limits->max_x);
            (void)snprintf(min_y_s, sizeof(min_y_s), "%f", limits->min_y);
            (void)snprintf(max_y_s, sizeof(max_y_s), "%f", limits->max_y);

            execlp(program_name, program_name, goal_str, dist_str,
                   min_x_s, max_x_s, min_y_s, max_y_s, (char *)NULL);

            /* Only reached if execlp fails */
            error_exit("execlp(3) failed in child", NULL, NULL, NULL);
            assert(0); /* Guideline 25 */
            break;

        default: /* Parent process */
            children[idx].pid = pid;
            break;
    }
}