#pragma once

void status_ok(char *oper, char *uri, int r_id); //200

void status_created(char *oper, char *uri, int r_id); //201

void status_bad(char *oper, char *uri, int r_id); //400

void status_forbidden(char *oper, char *uri, int r_id); //403

void status_notFound(char *oper, char *uri, int r_id); //404

void status_internal(char *oper, char *uri, int r_id); //500

void status_notImplemented(char *oper, char *uri, int r_id); //501
