#include "handlers_shared.h"

#include "../core/memory.h"
#include "../core/utils.h"
#include "../data/db.h"

#include <cwist/core/sstring/sstring.h>
#include <cwist/net/http/query.h>
#include <cjson/cJSON.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void login_handler(cwist_http_request *req, cwist_http_response *res) {
    cJSON *json = cJSON_Parse(req->body->data);
    if (!json) {
        res->status_code = 400;
        return;
    }

    cJSON *user_item = cJSON_GetObjectItem(json, "username");
    cJSON *pass_item = cJSON_GetObjectItem(json, "password");
    if (!user_item || !pass_item || !user_item->valuestring || !pass_item->valuestring) {
        res->status_code = 400;
        cJSON_Delete(json);
        return;
    }

    const char *username = user_item->valuestring;
    const char *password = pass_item->valuestring;
    for (int i = 0; username[i]; i++) {
        if (!isalnum((unsigned char)username[i])) {
            res->status_code = 400;
            cwist_sstring_assign(res->body, "{\"error\": \"Only alphanumeric names allowed\"}");
            cJSON_Delete(json);
            return;
        }
    }

    char hash[65];
    hash_password(password, hash);
    int uid = db_login_user(req->db, username, hash);

    cJSON *reply = cJSON_CreateObject();
    if (uid > 0) {
        cJSON_AddNumberToObject(reply, "user_id", uid);
        cJSON_AddStringToObject(reply, "username", username);
    } else {
        cJSON_AddStringToObject(reply, "error", "Invalid credentials");
        res->status_code = 401;
    }

    char *str = cJSON_PrintUnformatted(reply);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(reply);
    cJSON_Delete(json);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void register_handler(cwist_http_request *req, cwist_http_response *res) {
    cJSON *json = cJSON_Parse(req->body->data);
    if (!json) {
        res->status_code = 400;
        return;
    }

    cJSON *user_item = cJSON_GetObjectItem(json, "username");
    cJSON *pass_item = cJSON_GetObjectItem(json, "password");
    if (!user_item || !pass_item || !user_item->valuestring || !pass_item->valuestring ||
        strlen(user_item->valuestring) == 0 || strlen(pass_item->valuestring) == 0) {
        cJSON *reply = cJSON_CreateObject();
        cJSON_AddStringToObject(reply, "error", "Username and password are required");
        char *str = cJSON_PrintUnformatted(reply);
        cwist_sstring_assign(res->body, str);
        cev_mem_free(str);
        cJSON_Delete(reply);
        res->status_code = 400;
        cJSON_Delete(json);
        cwist_http_header_add(&res->headers, "Content-Type", "application/json");
        return;
    }

    const char *user = user_item->valuestring;
    const char *pass = pass_item->valuestring;
    for (int i = 0; user[i]; i++) {
        if (!isalnum((unsigned char)user[i])) {
            res->status_code = 400;
            cwist_sstring_assign(res->body, "{\"error\": \"Only alphanumeric names allowed\"}");
            cJSON_Delete(json);
            return;
        }
    }

    if (strlen(user) < 3 || strlen(pass) < 4) {
        cJSON *reply = cJSON_CreateObject();
        cJSON_AddStringToObject(reply, "error", "Username (min 3) or password (min 4) too short");
        char *str = cJSON_PrintUnformatted(reply);
        cwist_sstring_assign(res->body, str);
        cev_mem_free(str);
        cJSON_Delete(reply);
        res->status_code = 400;
        cJSON_Delete(json);
        cwist_http_header_add(&res->headers, "Content-Type", "application/json");
        return;
    }

    char hash[65];
    hash_password(pass, hash);
    int reg_res = db_register_user(req->db, user, hash);

    cJSON *reply = cJSON_CreateObject();
    if (reg_res == 0) {
        cJSON_AddStringToObject(reply, "status", "ok");
    } else {
        cJSON_AddStringToObject(reply, "error", "Username already exists");
        res->status_code = 409;
    }

    char *str = cJSON_PrintUnformatted(reply);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(reply);
    cJSON_Delete(json);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void rankings_handler(cwist_http_request *req, cwist_http_response *res) {
    cJSON *ranks = db_get_rankings(req->db);
    char *str = cJSON_PrintUnformatted(ranks);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(ranks);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void user_info_handler(cwist_http_request *req, cwist_http_response *res) {
    const char *uid_str = cwist_query_map_get(req->query_params, "user_id");
    if (!uid_str) {
        res->status_code = 400;
        return;
    }

    cJSON *info = db_get_user_info(req->db, atoi(uid_str));
    if (info) {
        char *str = cJSON_PrintUnformatted(info);
        cwist_sstring_assign(res->body, str);
        cev_mem_free(str);
        cJSON_Delete(info);
    } else {
        res->status_code = 404;
    }
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}

void rooms_handler(cwist_http_request *req, cwist_http_response *res) {
    cJSON *rooms = db_get_multiplayer_rooms(req->db);
    char *str = cJSON_PrintUnformatted(rooms);
    cwist_sstring_assign(res->body, str);
    cev_mem_free(str);
    cJSON_Delete(rooms);
    cwist_http_header_add(&res->headers, "Content-Type", "application/json");
}
