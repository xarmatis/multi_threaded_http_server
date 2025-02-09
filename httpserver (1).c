// Asgn 2: A simple HTTP server.
// By: Eugene Chou
//     Andrew Quinn
//     Brian Zhao

#include "audit.h"
#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "hash.h"
#include "rwlock.h"
#include "request.h"
#include "threadpool.h"
#include "protocol.h"
#include "httpserver.h"
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/stat.h>
#include <regex.h>
//void handle_connection(int);
//void handle_get(conn_t *);
//void handle_put(conn_t *);
//void handle_unsupported(conn_t *);
// globals
#define MY_URI "([a-zA-Z0-9.-]{1,63})"

PRIORITY priority = N_WAY;
uint32_t n = 0;

int main(int argc, char **argv) {
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Use getopt() to parse the command-line arguments */

    int opt;
    int num_threads = 4; //default
    char *end = NULL;

    while ((opt = getopt(argc, argv, "t:")) != -1) {

        switch (opt) {

        case 't':
            num_threads = (int) strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0') {

                fprintf(stderr, "Invalid thread count: %s\n", optarg);
                return EXIT_FAILURE;
            }
            if (num_threads <= 0) {

                fprintf(stderr, "Thread count must be greater than 0\n");
                return EXIT_FAILURE;
            }

            break;

        case '?':

            fprintf(stderr, "Usage: %s <-t numthreads> <port#>\n", argv[0]);
            return EXIT_FAILURE;
        default: break;
        }
    }

    if (optind >= argc) {

        fprintf(stderr, "Missing port number\n");
        fprintf(stderr, "Usage: %s <-t numthreads> <port#>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[optind], &endptr, 10);
    if (endptr && *endptr != '\0') {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    if (listener_init(&sock, port) < 0) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    /* Initialize worker threads, the queue, and other data structures */
    thread_pool_t *pool = thread_pool_create(num_threads, 200); //queue size will be 2x
    n = num_threads - 1;
    /* Hint: You will need to change how handle_connection() is used */
    while (1) {
        int connfd = listener_accept(&sock);
        //handle_connection(connfd);
        //close(connfd);
        if (!thread_pool_add_task(pool, (void *) (intptr_t) connfd)) {

            fprintf(stderr, "Failed to enqueue connection\n");
            close(connfd);
        }
    }

    return EXIT_SUCCESS;
}

void handle_connection(int connfd) {
    conn_t *conn = conn_new(connfd);

    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        //debug("%s", conn_str(conn)); //PRINTS THE HANDYT CONN STR
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
    close(connfd);
}

void handle_get(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    int r_id;
    char *r_id_str = conn_get_header(conn, "Request-Id");

    if (r_id_str == NULL) {

        r_id = 0;
    } else {

        r_id = atoi(r_id_str);
    }

    //printf("%s\n",uri);
    //debug("Handling GET request for %s", uri);
    //CHECK TO MAKE SURE URI IS VALID GIVEN THE REGEX

    regex_t regex;
    int result;

    if (regcomp(&regex, MY_URI, REG_EXTENDED) != 0) {

        status_internal("GET", uri, r_id);
        return;
    }
    result = regexec(&regex, uri, 0, NULL, 0);
    regfree(&regex);

    //invalid uri format
    if (result != 0) {

        status_forbidden("GET", uri, r_id);
        conn_send_response(conn, &RESPONSE_FORBIDDEN);
        return;

    } //TODO ADD THE PROPER CODE HERE TO RETURN RESULT and close connection

    // What are the steps in here?

    // 1. Open the file.
    rwlock_t *lock = get_lock_for_uri(uri, priority, n);
    reader_lock(lock);
    int fd = open(uri, O_RDONLY);

    // If open() returns < 0, then use the result appropriately
    //   a. Cannot access -- use RESPONSE_FORBIDDEN
    //   b. Cannot find the file -- use RESPONSE_NOT_FOUND
    //   c. Other error? -- use RESPONSE_INTERNAL_SERVER_ERROR
    // (Hint: Check errno for these cases)!

    if (fd < 0) {
        reader_unlock(lock);

        switch (errno) {

        case ENOENT:
            status_notFound("GET", uri, r_id);
            conn_send_response(conn, &RESPONSE_NOT_FOUND);
            break;
        case EACCES:
            status_forbidden("GET", uri, r_id);
            conn_send_response(conn, &RESPONSE_FORBIDDEN);
            break;
        default:
            status_internal("GET", uri, r_id);
            conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
            break;
        }

        return;
    }

    // 2. Get the size of the file.
    // (Hint: Checkout the function fstat())!
    //

    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {

        reader_unlock(lock);
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        status_internal("GET", uri, r_id);
        close(fd);
        return;
    }

    off_t file_size = file_stat.st_size;

    // 3. Check if the file is a directory, because directories *will*
    // open, but are not valid.
    // (Hint: Checkout the macro "S_IFDIR", which you can use after you call fstat()!)
    if (S_ISDIR(file_stat.st_mode)) {
        reader_unlock(lock);
        status_forbidden("GET", uri, r_id);
        conn_send_response(conn, &RESPONSE_FORBIDDEN);
        close(fd);
        return;
    }

    // 4. Send the file
    // (Hint: Checkout the conn_send_file() function!)
    const Response_t *send_res = conn_send_file(conn, fd, file_size);

    if (send_res != NULL) {
        reader_unlock(lock);
        status_internal("GET", uri, r_id);
        conn_send_response(conn, send_res);
        close(fd);
        return;
    }

    // 5. Close the file
    status_ok("GET", uri, r_id);
    close(fd);
    reader_unlock(lock);
}
void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    bool isNew = false; //check to see if file is created, (diff code)
    int r_id;
    char *r_id_str = conn_get_header(conn, "Request-Id");

    if (r_id_str == NULL) {

        r_id = 0;
    } else {

        r_id = atoi(r_id_str);
    }
    regex_t regex;
    int result;

    if (regcomp(&regex, MY_URI, REG_EXTENDED) != 0) {

        status_internal("PUT", uri, r_id);
        return;
    }
    result = regexec(&regex, uri, 0, NULL, 0);
    regfree(&regex);

    //invalid uri format
    if (result != 0) {
        //fprintf(stderr, "DHWAUIDADA");
        status_forbidden("PUT", uri, r_id);
        conn_send_response(conn, &RESPONSE_FORBIDDEN);
        return;
    }

    // What are the steps in here?

    // 1. Check if file already exists before opening it.
    // (Hint: check the access() function)!

    struct stat file_stat;
    if (stat(uri, &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) { // It's a directory
        status_forbidden("PUT", uri, r_id);
        conn_send_response(conn, &RESPONSE_FORBIDDEN);
        return;
    }

    if (access(uri, W_OK) == -1) {

        if (errno != ENOENT) { //file exists but forbidden

            status_forbidden("PUT", uri, r_id);
            conn_send_response(conn, &RESPONSE_FORBIDDEN);
            return;
        }
    }

    char temp[256];
    snprintf(temp, sizeof(temp), "%s.tmp.%ld", uri, pthread_self()); //creates unique temp file

    // (Hint: Checkout the conn_recv_file() function)!

    //write to temp file
    int temp_fd = open(temp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (temp_fd < 0) {

        status_internal("PUT", uri, r_id);
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        return;
    }

    res = conn_recv_file(conn, temp_fd);

    // 4. Send the response
    // (Hint: Checkout the conn_send_response() function)!

    if (res != NULL) {
        unlink(temp);
        conn_send_response(conn, res);
        close(temp_fd);
        return;
    }
    close(temp_fd);
    // 5. Close the file

    ///////CRIITICAL SECTION
    rwlock_t *lock = get_lock_for_uri(uri, priority, n);
    writer_lock(lock);
    if (access(uri, F_OK) == -1) { // File doesn't exist
        isNew = true; // Mark the file as new if it doesn't exist
    }
    if (rename(temp, uri) < 0) {
        
	writer_unlock(lock);
        status_internal("PUT", uri, r_id);
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
	unlink(temp);
        return;
    }

    if (isNew) {

        status_created("PUT", uri, r_id);
        conn_send_response(conn, &RESPONSE_CREATED);
    } else {

        status_ok("PUT", uri, r_id);
        conn_send_response(conn, &RESPONSE_OK);
    }
    unlink(temp);
    writer_unlock(lock);
}
void handle_unsupported(conn_t *conn) {
    const Request_t *req = conn_get_request(conn);
    const char *badreq = request_get_str(req);

    char data[64];
    if (badreq == NULL) {
        snprintf(data, sizeof(data), "UNKNOWN_REQUEST");
    } else {
        snprintf(data, sizeof(data), "%s", badreq);
    }

    char *uri = conn_get_uri(conn);
    if (uri == NULL) {
        uri = "UNKNOWN_URI";
    }

    int r_id = 0;
    char *r_id_str = conn_get_header(conn, "Request-Id");
    if (r_id_str != NULL) {
        r_id = atoi(r_id_str);
        free(r_id_str); // Free dynamically allocated memory
    }

    // Send responses
    status_notImplemented(data, uri, r_id);
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
}

