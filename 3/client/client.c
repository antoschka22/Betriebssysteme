/**
 * @file src/client.c
 * @author Antonio Molina Gradischnig, 12421907
 * @date 2025-12-22
 *
 * @brief A simple http client.
 */

#include "http.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <regex.h>

/**
 * @brief A struct to save http response data.
 */
typedef struct {
    unsigned short status;
    char msg[48];
    char location[256];
    unsigned int chunked:1;
} opt_t;

static const char *pgm_name;
static regex_t header_re, field_re;

/**
 * @brief Writes the usage to stderr and exits the program.
 */
static void usage(void) {
    fprintf(stderr, "%s: usage: client [-p PORT] [ -o FILE | -d DIR ] URL\n", pgm_name);
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
 * @brief Creates a socket and connects it to the provided host.
 * @param host Host name.
 * @param port Port number.
 * @return socket file descriptor.
 */
static int connect_to_host(const char *host, unsigned short port) {
    int ret, fd;

    // get address info
    struct addrinfo hints, *result = NULL, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;
    // getaddrinfo "leaks" memory - it seems, that this is intended behaviour
    // https://bugzilla.redhat.com/show_bug.cgi?id=116526
    // https://stackoverflow.com/questions/13229913/getaddrinfo-memory-leak
    if ((ret = getaddrinfo(host, "http", &hints, &result)) != 0) {
        fprintf(stderr, "%s: %s: %s\n", pgm_name, "Unable to get address info", gai_strerror(ret));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        // create socket
        if ((fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
            error("Unable to create socket");
            continue;
        }

        // set socket timeout
        struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1 ||
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1)
        {
            error("Unable to set timeout for socket");
            close(fd);
            continue;
        }

        // set port
        if (rp->ai_family == AF_INET6) {
            ((struct sockaddr_in6 *) rp->ai_addr)->sin6_port = htons(port);
        } else if (rp->ai_family == AF_INET) {
            ((struct sockaddr_in *) rp->ai_addr)->sin_port = htons(port);
        }

        // connect to host
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == -1) {
            error("Unable to connect socket");
            close(fd);
            continue;
        }

        break;
    }

    // cleanup
    freeaddrinfo(result);

    return (rp == NULL) ? -1 : fd;
}

/**
 * @brief Parses a http header.
 * @param buf A null terminated string, with an http header at the beginning.
 * @param opt A struct to be filled with information.
 * @return on success, the header size in bytes, otherwise -1.
 */
static int http_parse_header(char *buf, opt_t *opt) {
    regmatch_t match[4];
    if (regexec(&header_re, buf, 4, match, 0) != 0)
        return -1;

    opt->status = (buf[match[2].rm_so] - '0') * 100 + (buf[match[2].rm_so + 1] - '0') * 10 + (buf[match[2].rm_so + 2] - '0');
    sprintf(opt->msg, "%.*s", match[3].rm_eo - match[3].rm_so, buf + match[3].rm_so);

    opt->chunked = 0;
    opt->location[0] = 0;
    char *ptr = buf + match[3].rm_eo + 2;
    regmatch_t field_match[4];
    while (regexec(&field_re, ptr, 4, field_match, 0) == 0) {
        if (strncasecmp(ptr + field_match[1].rm_so, "Transfer-Encoding", 17) == 0) {
            if (strstr(ptr + field_match[2].rm_so, "chunked") <= ptr + field_match[2].rm_eo - 7) {
                opt->chunked = 1;
            }
        } else if (strncasecmp(ptr + field_match[1].rm_so, "Location", 8) == 0) {
            snprintf(opt->location, sizeof(opt->location), "%.*s", field_match[2].rm_eo - field_match[2].rm_so, ptr + field_match[2].rm_so);
        }
        ptr += field_match[0].rm_eo;
    }

    return match[0].rm_eo;
}

/**
 * @brief Converts a hex number to an integer.
 * @param ch Hex character.
 * @return on success, the converted number, otherwise -1.
 */
static int xtoi(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    } else if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    } else {
        return -1;
    }
}

/**
 * @brief Program entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit code.
 */
int main(int argc, char *const argv[]) {
    pgm_name = argv[0];

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // defaults
    int ret_code = EXIT_SUCCESS;
    unsigned short port = 80;
    char *file_name = NULL, *dir_name = NULL, *url = NULL, *path = NULL, host[256];

    // argument handling
    for (int opt; (opt = getopt(argc, argv, "hp:o:d:")) != -1;) {
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
            case 'o': file_name = optarg; break;
            case 'd': dir_name = optarg; break;
            case '?':
            default:
                usage();
        }
    }

    if (file_name && dir_name)
        usage();

    if (optind != argc - 1)
        usage();

    url = argv[optind++];

    if (strlen(url) <= 7 || strncmp(url, "http://", 7) != 0) {
        error("Invalid url");
        usage();
    }

    path = strpbrk(url + 7, ";/?:@=&");
    sprintf(host, "%.*s", (int) (path ? (path - (url + 7)) : strlen(url + 7)), url + 7);
    if (!path) path = "/";

    FILE *file = NULL;
    if (file_name) {
        if ((file = fopen(file_name, "wb+")) == NULL) {
            error("Unable to open file");
            return EXIT_FAILURE;
        }
    } else if (dir_name) {
        char buf[256];
        char *pos = strrchr(path, '/');
        char *qpos = strchr(pos, '?');
        char *name = (pos[1] == 0 || pos[1] == '?') ? "index.html" : pos + 1;
        sprintf(buf, "%s/%.*s", dir_name, (int) ((name != pos + 1 || qpos == NULL) ? strlen(name) : qpos - pos), name);
        if ((file = fopen(buf, "wb+")) == NULL) {
            error("Unable to open file");
            return EXIT_FAILURE;
        }
    }

    // compile regular expressions
    if (regcomp(&header_re, HTTP_RE_RESPONSE_HEADER, REG_EXTENDED) != 0) {
        error("Unable to compile regex");
        if (file) fclose(file);
        return EXIT_FAILURE;
    }
    if (regcomp(&field_re, "^" HTTP_RE_HEADER_FIELD "\r\n", REG_EXTENDED) != 0) {
        error("Unable to compile regex");
        if (file) fclose(file);
        regfree(&header_re);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "GET http://%s:%i%s\n", host, port, path);

    int sock;
    if ((sock = connect_to_host(host, port)) == -1) {
        if (file) fclose(file);
        regfree(&header_re);
        regfree(&field_re);
        return EXIT_FAILURE;
    }

    char buf[4096];
    opt_t http;

    // build request
    long len = snprintf(buf, sizeof(buf),
                        "GET %s HTTP/1.1\r\n"
                        "Host: %s\r\n"
                        "User-Agent: " HTTP_LIB "\r\n"
                        "Connection: close\r\n"
                        "\r\n", path, host);

    // send request
    if (send(sock, buf, len, 0) != len) {
        error("Unable to send to socket");
        ret_code = EXIT_FAILURE;
        goto abort;
    }

    // receive first bytes from socket (but only "peek")
    if ((len = recv(sock, buf, sizeof(buf) - 1, MSG_PEEK)) == -1) {
        error("Unable to receive from socket");
        ret_code = EXIT_FAILURE;
        goto abort;
    }

    // terminate string
    buf[len] = 0;

    // parse header
    if ((len = http_parse_header(buf, &http)) == -1) {
        fprintf(stderr, "Protocol error!\n");
        ret_code = 2;
        goto abort;
    } else {
        // receive complete header
        if (recv(sock, buf, len, 0) != len) {
            error("Unable to receive from socket");
            ret_code = EXIT_FAILURE;
            goto abort;
        }
    }

    fprintf(stderr, "%3i %s\n", http.status, http.msg);
    if (http.status != 200)
        ret_code = 3;
    if (http.status >= 300 && http.status < 400 && http.location[0] != 0)
        fprintf(stderr, "Location:%s\n", http.location);

    // receive payload
    long read;
    if (http.chunked) {
        // Transfer-Encoding: chunked
        long chunk_size, chunk_read;
        while ((read = recv(sock, buf, sizeof(buf), MSG_PEEK)) > 0) {
            // parse chunk size
            chunk_size = 0;
            int n = -1, i;
            for (i = 0; i < read - 1; i++) {
                if (buf[i] == '\r' && buf[i + 1] == '\n') break;
                if ((n = xtoi(buf[i])) < 0) break;
                chunk_size = (chunk_size << 4) | n;
            }

            if (n == -1 || i == 0) {
                fprintf(stderr, "Protocol error!\n");
                ret_code = 2;
                goto abort;
            }

            // receive chunk header
            if (recv(sock, buf, i + 2, 0) != i + 2) {
                read = -1;
                break;
            }

            // receive chunk
            chunk_read = 0;
            while ((read = recv(sock, buf, (chunk_size - chunk_read > sizeof(buf)) ? sizeof(buf) : chunk_size - chunk_read, 0)) > 0) {
                chunk_read += read;
                if (http.status == 200) {
                    fwrite(buf, 1, read, file ? file : stdout);
                }
            }
            if (read == -1) break;

            // receive chunk trailer
            read = recv(sock, buf, 2, 0);
            if (read != 2 || buf[0] != '\r' || buf[1] != '\n') {
                fprintf(stderr, "Protocol error!\n");
                ret_code = 2;
                goto abort;
            }
        }
    } else {
        // Transfer-Encoding is not chunked (="normal")
        while ((read = recv(sock, buf, sizeof(buf), 0)) > 0) {
            if (http.status == 200) {
                fwrite(buf, 1, read, file ? file : stdout);
            }
        }
    }

    if (read == -1) {
        error("Unable to receive from socket");
        ret_code = EXIT_FAILURE;
        goto abort;
    }

    abort:
    // cleanup
    close(sock);
    if (file) fclose(file);
    regfree(&header_re);
    regfree(&field_re);

    return ret_code;
}
