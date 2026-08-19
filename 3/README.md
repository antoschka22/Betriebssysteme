# Exercise 3: HTTP Client and Server

## Task
This project requires writing a client program and a server program which partially implement version 1.1 of the HTTP

## Client Implementation
* The client takes an URL as input, connects to the corresponding server and requests the file specified in the URL
* It uses the HTTP method GET
* The transmitted content is written to `stdout` or to a specified file or directory
* The client correctly parses the status code in the first line of the response

## Server Implementation
* The server waits for connections from clients and transmits the requested files
* It prepends a document root path to the requested path
* If the request method is not GET, it sends a response header with status code 501
* If the file cannot be opened, it sends a response header with status code 404
* Upon successful transmission, it sends a response header with status code 200
* The server handles the signals `SIGINT` and `SIGTERM` to complete ongoing requests before terminating