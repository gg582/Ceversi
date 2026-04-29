#include "handlers_shared.h"

#include "../data/db.h"
#include "../core/memory.h"

#include <cwist/core/sstring/sstring.h>
#include <cwist/core/utils/json_builder.h>
#include <cwist/net/http/query.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void join_handler(cwist_http_request *req, cwist_http_response *res) {
    int room_id = get_room_id(req);
    int pid;
    char mode[16];
    const char *requested_mode = cwist_query_map_get(req->query_params, "mode");
    const char *user_id_str = cwist_query_map_get(req->query_params, "user_id");
    int user_id = user_id_str ? atoi(user_id_str) : 0;

    if (db_join_game(req->db, room_id, requested_mode, &pid, mode, user_id) < 0) {
        res->status_code = CWIST_HTTP_FORBIDDEN;
        cwist_sstring_assign(res->body, "{\"error\": \"Room full\"}");
        return;
    }

    cwist_json_builder *jb = cwist_json_builder_create();
    cwist_json_begin_object(jb);
    cwist_json_add_int(jb, "player_id", pid);
    cwist_json_add_int(jb, "room_id", room_id);
    cwist_json_add_string(jb, "mode", mode);
    cwist_json_end_object(jb);

    cwist_sstring_assign(res->body, (char *)cwist_json_get_raw(jb));
    cwist_json_builder_destroy(jb);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void leave_handler(cwist_http_request *req, cwist_http_response *res) {
    int room_id = get_room_id(req);
    const char *user_id_str = cwist_query_map_get(req->query_params, "user_id");
    const char *player_id_str = cwist_query_map_get(req->query_params, "player_id");
    int user_id = user_id_str ? atoi(user_id_str) : 0;
    int player_id = player_id_str ? atoi(player_id_str) : 0;

    // db_leave_game now handles immediate DELETE for both games and sessions
    db_leave_game(req->db, room_id, player_id, user_id);

    cwist_sstring_assign(res->body, "{\"status\": \"SESSION_CLEANED\"}");
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void state_handler(cwist_http_request *req, cwist_http_response *res) {
    int room_id = get_room_id(req);
    int board[SIZE][SIZE];
    int turn, players;
    char status[32];
    char mode[16];
    get_game_state(req->db, room_id, board, &turn, status, &players, mode, NULL);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", status);
    cJSON_AddNumberToObject(json, "turn", turn);
    cJSON_AddStringToObject(json, "mode", mode);
    cJSON_AddNumberToObject(json, "room_id", room_id);

    cJSON *board_arr = cJSON_CreateArray();
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            cJSON_AddItemToArray(board_arr, cJSON_CreateNumber(board[r][c]));
        }
    }
    cJSON_AddItemToObject(json, "board", board_arr);

    char *str = cJSON_PrintUnformatted(json);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(json);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void move_handler(cwist_http_request *req, cwist_http_response *res) {
    int room_id = get_room_id(req);
    cJSON *json = cJSON_Parse(req->body->data);
    if (!json) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        return;
    }

    int r = cJSON_GetObjectItem(json, "r")->valueint;
    int c = cJSON_GetObjectItem(json, "c")->valueint;
    int p = cJSON_GetObjectItem(json, "player")->valueint;

    int board[SIZE][SIZE];
    int turn, players;
    char status[32];
    char mode[16];
    get_game_state(req->db, room_id, board, &turn, status, &players, mode, NULL);

    if (strcmp(status, "active") == 0 && p == turn) {
        int pieces = count_pieces(board);
        int is_reversi_setup = (strcmp(mode, "reversi") == 0 && pieces < 4);

        if (is_reversi_setup) {
            if (r < 3 || r > 4 || c < 3 || c > 4 || board[r][c] != 0) {
                res->status_code = CWIST_HTTP_BAD_REQUEST;
                cJSON_Delete(json);
                return;
            }
        } else if (!is_valid_move(board, r, c, p)) {
            res->status_code = CWIST_HTTP_BAD_REQUEST;
            cJSON_Delete(json);
            return;
        }

        board[r][c] = p;
        int opponent = (p == BLACK) ? WHITE : BLACK;

        if (!is_reversi_setup) {
            int dr[] = {-1, -1, -1, 0, 0, 1, 1, 1};
            int dc[] = {-1, 0, 1, -1, 1, -1, 0, 1};
            for (int i = 0; i < 8; i++) {
                int nr = r + dr[i];
                int nc = c + dc[i];
                int r_temp = nr;
                int c_temp = nc;
                int count = 0;
                while (r_temp >= 0 && r_temp < SIZE && c_temp >= 0 && c_temp < SIZE &&
                       board[r_temp][c_temp] == opponent) {
                    r_temp += dr[i];
                    c_temp += dc[i];
                    count++;
                }
                if (count > 0 && r_temp >= 0 && r_temp < SIZE && c_temp >= 0 && c_temp < SIZE &&
                    board[r_temp][c_temp] == p) {
                    int rr = r + dr[i];
                    int cc = c + dc[i];
                    while (board[rr][cc] == opponent) {
                        board[rr][cc] = p;
                        rr += dr[i];
                        cc += dc[i];
                    }
                }
            }
        }

        if (is_reversi_setup && count_pieces(board) < 4) {
            turn = opponent;
        } else {
            if (has_valid_moves(board, opponent)) {
                turn = opponent;
            } else if (!has_valid_moves(board, p)) {
                strcpy(status, "finished");

                int b_cnt = 0;
                int w_cnt = 0;
                for (int rr = 0; rr < SIZE; rr++) {
                    for (int cc = 0; cc < SIZE; cc++) {
                        if (board[rr][cc] == BLACK) b_cnt++;
                        else if (board[rr][cc] == WHITE) w_cnt++;
                    }
                }
                int winner = (b_cnt > w_cnt) ? 1 : (w_cnt > b_cnt ? 2 : 0);
                db_record_result(req->db, room_id, winner);
                db_settle_multiplayer_bets(req->db, room_id, winner, NULL);
            }
        }

        update_game_state(req->db, room_id, board, turn, status, players, mode);
        cwist_sstring_assign(res->body, "{\"status\":\"ok\"}");
    } else {
        res->status_code = CWIST_HTTP_FORBIDDEN;
    }

    cJSON_Delete(json);
}
