#include "handlers_shared.h"

#include "../core/memory.h"
#include "../data/db.h"

#include <cwist/core/html/builder.h>
#include <cwist/core/sstring/sstring.h>
#include <cwist/core/template/template.h>
#include <cwist/net/http/query.h>
#include <cjson/cJSON.h>
#include <stdlib.h>

void root_handler(cwist_http_request *req, cwist_http_response *res) {
    const char *room_str = cwist_query_map_get(req->query_params, "room");
    cJSON *context = cJSON_CreateObject();

    cJSON_AddNumberToObject(context, "room_id", 0);
    cJSON_AddStringToObject(context, "mode", "othello");
    cJSON_AddStringToObject(context, "status", "waiting");
    cJSON_AddNumberToObject(context, "turn_val", 1);
    cJSON_AddStringToObject(context, "turn_text", "Black's Turn");
    cJSON_AddNumberToObject(context, "score_black", 0);
    cJSON_AddNumberToObject(context, "score_white", 0);
    cJSON_AddStringToObject(context, "board_html", "");
    cJSON_AddStringToObject(context, "board_json", "[]");

    if (room_str) {
        int room_id = atoi(room_str);
        int board[SIZE][SIZE];
        int turn, players;
        char status[32];
        char mode[16];

        get_game_state(req->db, room_id, board, &turn, status, &players, mode, NULL);

        cJSON_ReplaceItemInObject(context, "room_id", cJSON_CreateNumber(room_id));
        cJSON_ReplaceItemInObject(context, "mode", cJSON_CreateString(mode));
        cJSON_ReplaceItemInObject(context, "status", cJSON_CreateString(status));
        cJSON_ReplaceItemInObject(context, "turn_val", cJSON_CreateNumber(turn));
        cJSON_ReplaceItemInObject(
            context,
            "turn_text",
            cJSON_CreateString(turn == 1 ? "Black's Turn" : "White's Turn")
        );

        int black_score = 0;
        int white_score = 0;
        for (int r = 0; r < SIZE; r++) {
            for (int c = 0; c < SIZE; c++) {
                if (board[r][c] == BLACK) black_score++;
                else if (board[r][c] == WHITE) white_score++;
            }
        }
        cJSON_ReplaceItemInObject(context, "score_black", cJSON_CreateNumber(black_score));
        cJSON_ReplaceItemInObject(context, "score_white", cJSON_CreateNumber(white_score));

        cJSON *board_arr = cJSON_CreateArray();
        for (int r = 0; r < SIZE; r++) {
            for (int c = 0; c < SIZE; c++) {
                cJSON_AddItemToArray(board_arr, cJSON_CreateNumber(board[r][c]));
            }
        }

        char *board_json_str = cJSON_PrintUnformatted(board_arr);
        cJSON_ReplaceItemInObject(context, "board_json", cJSON_CreateString(board_json_str));
        cev_mem_free(board_json_str);
        cJSON_Delete(board_arr);

        cwist_sstring *cells_html = cwist_sstring_create();
        cwist_sstring_assign(cells_html, "");
        for (int r = 0; r < SIZE; r++) {
            for (int c = 0; c < SIZE; c++) {
                cwist_html_element_t *cell = cwist_html_element_create("div");
                cwist_html_element_add_class(cell, "cell");
                if (board[r][c] != 0) {
                    cwist_html_element_t *disc = cwist_html_element_create("div");
                    cwist_html_element_add_class(disc, "disc");
                    cwist_html_element_add_class(disc, board[r][c] == BLACK ? "black" : "white");
                    cwist_html_element_add_child(cell, disc);
                }

                cwist_sstring *cell_str = cwist_html_render(cell);
                cwist_sstring_append_sstring(cells_html, cell_str);
                cwist_sstring_destroy(cell_str);
                cwist_html_element_destroy(cell);
            }
        }

        cJSON_ReplaceItemInObject(context, "board_html", cJSON_CreateString(cells_html->data));
        cwist_sstring_destroy(cells_html);
    }

    cwist_sstring *rendered = cwist_template_render_file("templates/index.html.tmpl", context);
    if (rendered) {
        cwist_sstring_assign(res->body, rendered->data);
        cwist_sstring_destroy(rendered);
    } else {
        res->status_code = 500;
        cwist_sstring_assign(res->body, "Template Error");
    }

    cJSON_Delete(context);
    cwist_http_header_add(&res->headers, "Content-Type", "text/html");
}
