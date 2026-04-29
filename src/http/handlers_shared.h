#ifndef HANDLERS_SHARED_H
#define HANDLERS_SHARED_H

#include "handlers.h"

#include "../core/common.h"

#include <cjson/cJSON.h>

int is_valid_move(int board[SIZE][SIZE], int r, int c, int p);
int has_valid_moves(int board[SIZE][SIZE], int p);
int count_pieces(int board[SIZE][SIZE]);
int get_room_id(cwist_http_request *req);
void build_session_identity(cwist_http_request *req, char *identity, size_t n);
void build_identity(cwist_http_request *req, char *identity, size_t n);
void build_identity_from_json(cJSON *user_item, cJSON *guest_item, char *identity, size_t n);
int parse_positive_int_or_default(const char *s, int fallback);

#endif
