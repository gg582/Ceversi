#include "handlers_shared.h"

#include "../core/memory.h"
#include "../data/db.h"

#include <cwist/core/sstring/sstring.h>
#include <cwist/net/http/query.h>
#include <cjson/cJSON.h>
#include <string.h>

void sessions_handler(cwist_http_request *req, cwist_http_response *res) {
    char identity[128];
    build_session_identity(req, identity, sizeof(identity));
    const char *type = cwist_query_map_get(req->query_params, "type");
    if (!type || (strcmp(type, "singleplayer") != 0 && strcmp(type, "multiplayer") != 0)) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        cwist_sstring_assign(res->body, "{\"error\":\"type must be singleplayer or multiplayer\"}");
        cwist_http_header_add(&res->headers, "Content-Type", "application/json");
        return;
    }

    const char *limit_str = cwist_query_map_get(req->query_params, "limit");
    int limit = parse_positive_int_or_default(limit_str, 8);
    cJSON *sessions = db_get_recent_sessions(req->db, identity, type, limit);

    cJSON *reply = cJSON_CreateObject();
    cJSON_AddStringToObject(reply, "identity", identity);
    cJSON_AddStringToObject(reply, "type", type);
    cJSON_AddItemToObject(reply, "sessions", sessions);

    char *str = cJSON_PrintUnformatted(reply);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(reply);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void sessions_log_handler(cwist_http_request *req, cwist_http_response *res) {
    cJSON *json = cJSON_Parse(req->body->data);
    if (!json) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        return;
    }

    cJSON *guest_item = cJSON_GetObjectItem(json, "guest_id");
    cJSON *user_item = cJSON_GetObjectItem(json, "user_id");
    cJSON *type_item = cJSON_GetObjectItem(json, "session_type");
    cJSON *mode_item = cJSON_GetObjectItem(json, "mode");
    cJSON *difficulty_item = cJSON_GetObjectItem(json, "difficulty");
    cJSON *room_item = cJSON_GetObjectItem(json, "room_id");

    if (!type_item || !type_item->valuestring) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        cJSON_Delete(json);
        return;
    }
    if (strcmp(type_item->valuestring, "singleplayer") != 0 &&
        strcmp(type_item->valuestring, "multiplayer") != 0) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        cJSON_Delete(json);
        return;
    }

    char identity[128];
    build_identity_from_json(user_item, guest_item, identity, sizeof(identity));

    const char *mode = (mode_item && mode_item->valuestring) ? mode_item->valuestring : "othello";
    const char *difficulty = (difficulty_item && difficulty_item->valuestring) ? difficulty_item->valuestring : "";
    int room_id = (room_item && cJSON_IsNumber(room_item)) ? room_item->valueint : 0;
    int rc = db_log_game_session(req->db, identity, type_item->valuestring, mode, difficulty, room_id);
    if (rc != 0) {
        res->status_code = CWIST_HTTP_BAD_REQUEST;
        cwist_sstring_assign(res->body, "{\"error\":\"failed to log session\"}");
        cJSON_Delete(json);
        cwist_http_header_add(&res->headers, "Content-Type", "application/json");
        return;
    }

    cJSON *reply = cJSON_CreateObject();
    cJSON_AddStringToObject(reply, "status", "ok");
    cJSON_AddStringToObject(reply, "identity", identity);

    char *str = cJSON_PrintUnformatted(reply);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(reply);
    cJSON_Delete(json);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}
