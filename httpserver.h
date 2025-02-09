#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "connection.h" // Include conn_t definition

void handle_connection(int connfd);
void handle_get(conn_t *conn);
void handle_put(conn_t *conn);
void handle_unsupported(conn_t *conn);

#endif // HTTPSERVER_H
