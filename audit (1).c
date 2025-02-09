#include <stdio.h>
#include "audit.h"

void status_ok(char *oper, char *uri, int r_id) {
    fprintf(stderr, "%s,%s,200,%d\n", oper, uri, r_id);
}

void status_created(char *oper, char *uri, int r_id) {
    fprintf(stderr, "%s,%s,201,%d\n", oper, uri, r_id);
}

void status_bad(char *oper, char *uri, int r_id) {
    fprintf(stderr, "%s,%s,400,%d\n", oper, uri, r_id);
}

void status_forbidden(char *oper, char *uri, int r_id) {
    fprintf(stderr, "%s,%s,403,%d\n", oper, uri, r_id);
}

void status_notFound(char *oper, char *uri, int r_id) {
    fprintf(stderr, "%s,%s,404,%d\n", oper, uri, r_id);
}

void status_internal(char *oper, char *uri, int r_id) {
    fprintf(stderr, "%s,%s,500,%d\n", oper, uri, r_id);
}

void status_notImplemented(char *oper, char *uri, int r_id) {
    fprintf(stderr, "%s,%s,501,%d\n", oper, uri, r_id);
}
