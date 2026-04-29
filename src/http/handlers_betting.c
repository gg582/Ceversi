#include "handlers_shared.h"

#include "../core/memory.h"
#include "../data/db.h"
#include "../game/betting_logic.h"

#include <cwist/core/sstring/sstring.h>
#include <cwist/net/http/query.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

void betting_enter_handler(cwist_http_request *req, cwist_http_response *res) {
    char identity[128];
    build_identity(req, identity, sizeof(identity));
    int points = BETTING_START_POINTS;
    if (db_get_betting_points(req->db, identity, &points) != 0) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        return;
    }

    cJSON *reply = cJSON_CreateObject();
    cJSON_AddStringToObject(reply, "identity", identity);
    cJSON_AddNumberToObject(reply, "points", points);
    char *str = cJSON_PrintUnformatted(reply);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(reply);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void betting_slots_handler(cwist_http_request *req, cwist_http_response *res) {
    cJSON *slots = db_get_betting_slots(req->db);
    cJSON *reply = cJSON_CreateObject();
    cJSON_AddItemToObject(reply, "slots", slots);

    char *str = cJSON_PrintUnformatted(reply);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(reply);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void betting_rankings_handler(cwist_http_request *req, cwist_http_response *res) {
    cJSON *ranks = db_get_betting_rankings(req->db);
    cJSON *reply = cJSON_CreateObject();
    cJSON_AddItemToObject(reply, "rankings", ranks);
    char *str = cJSON_PrintUnformatted(reply);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(reply);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void betting_place_handler(cwist_http_request *req, cwist_http_response *res) {
    cJSON *json = cJSON_Parse(req->body->data);
    if (!json) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        return;
    }

    cJSON *slot_item = cJSON_GetObjectItem(json, "slot_id");
    cJSON *outcome_item = cJSON_GetObjectItem(json, "outcome");
    cJSON *amount_item = cJSON_GetObjectItem(json, "amount");
    cJSON *guest_item = cJSON_GetObjectItem(json, "guest_id");
    cJSON *user_item = cJSON_GetObjectItem(json, "user_id");
    char identity[128];
    build_identity_from_json(user_item, guest_item, identity, sizeof(identity));

    if (!slot_item || !outcome_item || !amount_item || !outcome_item->valuestring) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        cJSON_Delete(json);
        return;
    }

    cJSON *result = NULL;
    int rc = db_apply_bet(
        req->db,
        identity,
        slot_item->valueint,
        outcome_item->valuestring,
        amount_item->valueint,
        &result
    );
    if (rc != 0) {
        cJSON *err = cJSON_CreateObject();
        if (rc == -3) cJSON_AddStringToObject(err, "error", "Bet amount exceeds current points");
        else cJSON_AddStringToObject(err, "error", "Invalid bet request");
        char *err_str = cJSON_PrintUnformatted(err);
        cwist_sstring_assign(res->body, err_str);
        cev_mem_free(err_str);
        cJSON_Delete(err);
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        cJSON_Delete(json);
        return;
    }

    char *str = cJSON_PrintUnformatted(result);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(result);
    cJSON_Delete(json);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void betting_multiplayer_place_handler(cwist_http_request *req, cwist_http_response *res) {
    cJSON *json = cJSON_Parse(req->body->data);
    if (!json) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        return;
    }

    cJSON *room_item = cJSON_GetObjectItem(json, "room_id");
    cJSON *target_item = cJSON_GetObjectItem(json, "target_player");
    cJSON *amount_item = cJSON_GetObjectItem(json, "amount");
    cJSON *guest_item = cJSON_GetObjectItem(json, "guest_id");
    cJSON *user_item = cJSON_GetObjectItem(json, "user_id");
    if (!room_item || !target_item || !amount_item ||
        !cJSON_IsNumber(room_item) || !cJSON_IsNumber(target_item) || !cJSON_IsNumber(amount_item)) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        cJSON_Delete(json);
        return;
    }

    char identity[128];
    build_identity_from_json(user_item, guest_item, identity, sizeof(identity));

    cJSON *result = NULL;
    int rc = db_place_multiplayer_bet(
        req->db,
        identity,
        room_item->valueint,
        target_item->valueint,
        amount_item->valueint,
        &result
    );
    if (rc != 0) {
        cJSON *err = cJSON_CreateObject();
        if (rc == -3) cJSON_AddStringToObject(err, "error", "Bet amount exceeds allowed point range");
        else cJSON_AddStringToObject(err, "error", "Invalid multiplayer bet request");
        char *err_str = cJSON_PrintUnformatted(err);
        cwist_sstring_assign(res->body, err_str);
        cev_mem_free(err_str);
        cJSON_Delete(err);
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        cJSON_Delete(json);
        return;
    }

    char *str = cJSON_PrintUnformatted(result);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(result);
    cJSON_Delete(json);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void betting_multiplayer_history_handler(cwist_http_request *req, cwist_http_response *res) {
    const char *room_str = cwist_query_map_get(req->query_params, "room_id");
    int room_id = room_str ? atoi(room_str) : 0;
    char identity[128];
    build_identity(req, identity, sizeof(identity));

    cJSON *history = db_get_multiplayer_bet_history(req->db, identity, room_id);
    cJSON *reply = cJSON_CreateObject();
    cJSON_AddStringToObject(reply, "identity", identity);
    cJSON_AddItemToObject(reply, "bets", history);

    char *str = cJSON_PrintUnformatted(reply);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(reply);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}
