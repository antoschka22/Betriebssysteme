/**
 * @file src/http.h
 * @author Antonio Molina Gradischnig, 12421907
 * @date 2025-12-22
 *
 * @brief HTTP definitions.
 */

#define HTTP_LIB                "osue-12119052/1.0"

#define HTTP_RE_LWS             "(\r\n)?[ \t]+"
#define HTTP_RE_TEXT            "[^\x01-\x08\x0A-\x19\x7F]*"
#define HTTP_RE_TOKEN           "[!#$%&'*+-.^_`|~0-9A-Za-z]+"
#define HTTP_RE_VERSION         "HTTP/([0-9]+\\.[0-9]+)"
#define HTTP_RE_HEADER_FIELD    "(" HTTP_RE_TOKEN "):((" HTTP_RE_LWS "|" HTTP_RE_TEXT ")*)"
#define HTTP_RE_STATUS_LINE     HTTP_RE_VERSION " ([0-9]{3}) (" HTTP_RE_TEXT ")\r\n"
#define HTTP_RE_REQUEST_LINE    "(" HTTP_RE_TOKEN ") ([^\x01-\x20\x7F]+) " HTTP_RE_VERSION "\r\n"

#define HTTP_RE_RESPONSE_HEADER "^" HTTP_RE_STATUS_LINE "(" HTTP_RE_HEADER_FIELD "\r\n)*\r\n"
#define HTTP_RE_REQUEST_HEADER  "^" HTTP_RE_REQUEST_LINE "(" HTTP_RE_HEADER_FIELD "\r\n)*\r\n"
