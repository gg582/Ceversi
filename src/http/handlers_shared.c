#include "handlers_shared.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_valid_move(int board[SIZE][SIZE], int r, int c, int p) {
    if (r < 0 || r >= SIZE || c < 0 || c >= SIZE || board[r][c] != 0) return 0;
    int opponent = (p == BLACK) ? WHITE : BLACK;
    int dr[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int dc[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    for (int i = 0; i < 8; i++) {
        int nr = r + dr[i];
        int nc = c + dc[i];
        int count = 0;
        while (nr >= 0 && nr < SIZE && nc >= 0 && nc < SIZE && board[nr][nc] == opponent) {
            nr += dr[i];
            nc += dc[i];
            count++;
        }
        if (count > 0 && nr >= 0 && nr < SIZE && nc >= 0 && nc < SIZE && board[nr][nc] == p) {
            return 1;
        }
    }
    return 0;
}

int has_valid_moves(int board[SIZE][SIZE], int p) {
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (is_valid_move(board, r, c, p)) return 1;
        }
    }
    return 0;
}

int count_pieces(int board[SIZE][SIZE]) {
    int count = 0;
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] != 0) count++;
        }
    }
    return count;
}

int get_room_id(cwist_http_request *req) {
    int room_id = 1;
    const char *room_str = cwist_query_map_get(req->query_params, "room");
    if (room_str && strlen(room_str) > 0) {
        room_id = atoi(room_str);
    }
    return room_id;
}

void build_session_identity(cwist_http_request *req, char *identity, size_t n) {
    const char *user_id_str = cwist_query_map_get(req->query_params, "user_id");
    const char *guest_id = cwist_query_map_get(req->query_params, "guest_id");
    int user_id = (user_id_str && strlen(user_id_str) > 0) ? atoi(user_id_str) : 0;
    if (user_id > 0) {
        snprintf(identity, n, "user:%d", user_id);
    } else if (guest_id && strlen(guest_id) > 0) {
        snprintf(identity, n, "guest:%s", guest_id);
    } else {
        snprintf(identity, n, "guest:default");
    }
}

void build_identity(cwist_http_request *req, char *identity, size_t n) {
    build_session_identity(req, identity, n);
}

int parse_positive_int_or_default(const char *s, int fallback) {
    if (!s || strlen(s) == 0) return fallback;
    for (int i = 0; s[i]; i++) {
        if (!isdigit((unsigned char)s[i])) return fallback;
    }
    int value = atoi(s);
    return value > 0 ? value : fallback;
}

void build_identity_from_json(cJSON *user_item, cJSON *guest_item, char *identity, size_t n) {
    if (user_item && cJSON_IsNumber(user_item) && user_item->valueint > 0) {
        snprintf(identity, n, "user:%d", user_item->valueint);
    } else if (guest_item && guest_item->valuestring && strlen(guest_item->valuestring) > 0) {
        snprintf(identity, n, "guest:%s", guest_item->valuestring);
    } else {
        snprintf(identity, n, "guest:default");
    }
}
