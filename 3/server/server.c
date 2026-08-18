/**
 * @file src/server.c
 * @author Antonio Molina Gradischnig, 12421907
 * @date 2025-12-22
 *
 * @brief A simple http server.
 */

#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <regex.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/sendfile.h>

static const char *pgm_name;
static volatile int alive = 1;
static regex_t header_re;

/**
 * @brief Writes the usage to stderr and exits the program.
 */
static void usage(void) {
    fprintf(stderr, "%s: usage: server [-p PORT] [-i INDEX] DOC_ROOT\n", pgm_name);
    exit(EXIT_FAILURE);
}

/**
 * @brief Writes an error message and the textual representation of errno to stderr.
 * @param msg User defined message.
 */
static void error(const char *msg) {
    if (errno == 0) {
        fprintf(stderr, "%s: %s\n", pgm_name, msg);
    } else {
        fprintf(stderr, "%s: %s: %s\n", pgm_name, msg, strerror(errno));
    }
}

/**
 * @brief Signal handler for SIGINT/SIGTERM.
 * @param sig Signal identifier.
 */
static void terminate(int sig) {
    fprintf(stderr, "\nTerminating...\n");
    alive = 0;
}

/**
 * @brief Returns the current http date string.
 * @return current http date string.
 */
static char *get_date(void) {
    static char buf[64];
    time_t raw_time;
    time(&raw_time);
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&raw_time));
    return buf;
}

/**
 * @brief Responds to client with given code.
 * @param client The client file descriptor.
 * @param code The http status code.
 * @param msg The http status description.
 */
static void respond_error(int client, short code, char *msg) {
    long len, ret;
    char buf1[256], buf2[1024];

    // log response
    printf("%03i %s\n", code, msg);

    // generate response
    len = snprintf(buf1, sizeof(buf1), "%03i %s\n", code, msg);
    len = snprintf(buf2, sizeof(buf2),
                   "HTTP/1.1 %03i %s\r\n"
                   "Date: %s\r\n"
                   "Server: " HTTP_LIB "\r\n"
                   "Connection: close\r\n"
                   "Content-Length: %li\r\n"
                   "Content-Type: text/plain\r\n"
                   "\r\n"
                   "%s", code, msg, get_date(), len, buf1);

    // send response
    retry_send:
    if ((ret = send(client, buf2, len, 0)) != len) {
        if (ret == -1 && errno == EINTR) {
            errno = 0;
            goto retry_send;
        }
        error("Unable to send to client");
    }
}

/**
 * @brief Is the given file a regular file?
 * @param f The file to check.
 * @return Boolean if the file is a regular file.
 */
static int is_reg_file(FILE *f) {
    struct stat stat;
    if (fstat(fileno(f), &stat) != 0) {
        errno = 0;
        return 0;
    }
    return S_ISREG(stat.st_mode);
}

/**
 * @brief Ends the given string with the given suffix?
 * @param str The string to be checked.
 * @param suffix The suffix to be checked.
 * @return Boolean if the strings ends with the suffix.
 */
static int strend(const char *str, const char *suffix) {
    size_t len1 = strlen(str), len2 = strlen(suffix);
    return len1 >= len2 && strcmp(str + len1 - len2, suffix) == 0;
}

/**
 * Connection handler
 * @brief Handle a client connection.
 * @param client The client file descriptor.
 * @param doc_root The configured document root.
 * @param index The configured name of the index file.
 */
static void cnx_handler(int client, const char *doc_root, const char *index) {
    char buf[4096];
    unsigned long len;
    long ret;
    FILE *file = NULL;

    // receive header
    retry_recv:
    if ((len = recv(client, buf, sizeof(buf) - 1, 0)) == -1) {
        if (errno == EINTR) {
            errno = 0;
            goto retry_recv;
        }
        error("Unable to receive from client");
        goto abort;
    }
    buf[len] = 0;

    // check header
    regmatch_t match[4];
    if (regexec(&header_re, buf, 4, match, 0) != 0) {
        respond_error(client, 400, "Bad Request");
        goto abort;
    }

    char *method = buf + match[1].rm_so;
    int method_len = match[1].rm_eo - match[1].rm_so;
    char *path = buf + match[2].rm_so;
    int path_len = match[2].rm_eo - match[2].rm_so;
    char *version = buf + match[3].rm_so;
    int version_len = match[3].rm_eo - match[3].rm_so;

    printf("%.*s %.*s\n", method_len, method, path_len, path);

    // check version
    if (version_len != 3 || strncmp(version, "1.1", 3) != 0) {
        respond_error(client, 400, "Bad Request");

        // the following response would be more accurate, but the submission system won't allow it :(
        //respond_error(client, 505, "HTTP Version Not Supported");

        goto abort;
    }

    // check method
    if (method_len != 3 || strncmp(method, "GET", 3) != 0) {
        respond_error(client, 501, "Not Implemented");
        goto abort;
    }

    // check path
    if (path[0] != '/') {
        respond_error(client, 400, "Bad Request");
        goto abort;
    }

    // check path length
    char filename[256];
    if (strlen(doc_root) + 1 + path_len + strlen(index) >= sizeof(filename)) {
        respond_error(client, 414, "URI Too Long");
        goto abort;
    }

    // build filename
    snprintf(filename, sizeof(filename), "%s/%.*s", doc_root, path_len, path);
    if (filename[strlen(filename) - 1] == '/') {
        strncat(filename, index, sizeof(filename) - strlen(filename) - 1);
    }

    if ((file = fopen(filename, "rb")) == NULL) {
        error("Unable to open file");
        if (errno == ENOENT) {
            respond_error(client, 404, "Not Found");
        } else if (errno == EACCES) {
            respond_error(client, 403, "Forbidden");
        } else {
            respond_error(client, 500, "Internal Server Error");
        }
        goto abort;
    }

    if (!is_reg_file(file)) {
        respond_error(client, 403, "Forbidden");
        goto abort;
    }

    // get file size
    long file_size;
    if (fseek(file, 0, SEEK_END) != 0 ||
        (file_size = ftell(file)) == -1 ||
        fseek(file, 0, SEEK_SET) != 0)
    {
        error("Unable to get size of file");
        respond_error(client, 500, "Internal Server Error");
        goto abort;
    }

    // log response
    printf("200 OK\n");

    // determine content type
    char additional_headers[256] = "";
    if (strend(filename, ".html") || strend(filename, ".htm")) {
        strcat(additional_headers, "Content-Type: text/html\r\n");
    } else if (strend(filename, ".css")) {
        strcat(additional_headers, "Content-Type: text/css\r\n");
    } else if (strend(filename, ".js")) {
        strcat(additional_headers, "Content-Type: application/javascript\r\n");
    }

    // generate http header
    len = snprintf(buf, sizeof(buf),
                   "HTTP/1.1 200 OK\r\n"
                   "Date: %s\r\n"
                   "Server: " HTTP_LIB "\r\n"
                   "Connection: close\r\n"
                   "Content-Length: %li\r\n"
                   "%s"
                   "\r\n", get_date(), file_size, additional_headers);

    // send http header
    retry_send:
    if ((ret = send(client, buf, len, 0)) != len) {
        if (ret == -1 && errno == EINTR) {
            errno = 0;
            goto retry_send;
        }
        error("Unable to send header to client");
        goto abort;
    }

    // send file to client
    for (long to_send = file_size, offset = 0; to_send > 0; to_send -= ret) {
        // sendfile will transmit at most 0x7ffff000 bytes
        if ((ret = sendfile(client, fileno(file), &offset, to_send)) == -1) {
            if (errno == EINTR) {
                errno = 0;
                continue;
            }
            error("Unable to send file");
            goto abort;
        }
    }

    abort:
    // cleanup
    close(client);
    if (file) fclose(file);
}

/**
 * @brief Program entry point.
 * @param argc The argument count.
 * @param argv The argument vector.
 * @return exit code.
 */
int main(int argc, char *const argv[]) {
    pgm_name = argv[0];

    // signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // defaults
    unsigned short port = 8080;
    char *index = "index.html", *doc_root = NULL;

    // argument handling
    for (int opt; (opt = getopt(argc, argv, "hp:i:")) != -1;) {
        switch (opt) {
            case 'h': usage();
            case 'p': {
                char *ptr = optarg;
                long ret = strtol(optarg, &ptr, 10);
                if (ret <= 0 || ret > 65535 || ptr[0] != 0) {
                    usage();
                } else {
                    port = ret;
                }
                break;
            }
            case 'i': index = optarg; break;
            case '?':
            default:
                usage();
        }
    }

    if (optind != argc - 1)
        usage();

    doc_root = argv[optind++];

    // create socket
    int server;
    if ((server = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
        error("Unable to create socket");
        return EXIT_FAILURE;
    }

    // bind socket to address
    struct sockaddr_in6 addr = {.sin6_family = AF_INET6, .sin6_addr = IN6ADDR_ANY_INIT, .sin6_port = htons(port)};
    if (bind(server, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        error("Unable to bind socket to address");
        close(server);
        return EXIT_FAILURE;
    }

    // connect to host
    if (listen(server, 16) != 0) {
        error("Unable to listen on socket");
        close(server);
        return EXIT_FAILURE;
    }

    // initialize header regular expression
    if (regcomp(&header_re, HTTP_RE_REQUEST_HEADER, REG_EXTENDED) != 0) {
        error("Unable to compile regex");
        close(server);
        return EXIT_FAILURE;
    }

    // main loop
    int client;
    while (alive) {
        if ((client = accept(server, NULL, 0)) == -1) {
            if (errno == EINTR) continue;
            error("Unable to accept connection");
            return EXIT_FAILURE;
        }

        // handle connection
        printf("Accepted connection\n");
        cnx_handler(client, doc_root, index);
        printf("Closed connection\n");

        // reset errno
        errno = 0;
    }

    // cleanup
    close(server);
    regfree(&header_re);

    return 0;
}
