#define _GNU_SOURCE
#include <cwist/sys/app/app.h>
#include <cwist/net/http/http.h>
#include <cwist/core/db/sql.h>
#include <cwist/core/db/nuke_db.h>
#include <cwist/core/macros.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include "common.h"
#include "db.h"
#include "handlers.h"
#include "utils.h"
#include "memory.h"

#define PORT 31744

void *cleanup_thread(void *arg) {
    cwist_db *db = (cwist_db *)arg;
    while(1) {
        sleep(60);
        cleanup_stale_rooms(db);
        cev_mem_collect();
    }
    return NULL;
}

int main(int argc, char **argv) {
    int port = PORT;
    char *port_env = getenv("PORT");
    if (port_env) {
        port = atoi(port_env);
    }

    int use_https = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-certs") == 0) {
            use_https = 0;
        }
    }

    signal(SIGPIPE, SIG_IGN);
    cev_mem_bootstrap();

    cwist_app *app = cwist_app_create();
    if (!app) {
        fprintf(stderr, "Failed to create cwist app\n");
        return 1;
    }

    const char *db_path = "othello.db";

    // --- Smart Features Integration ---
    // 1. Conservative Memory Limit for Static Files (Fail-safe)
    cwist_app_set_max_memspace(app, CWIST_MIB(128)); 

    // 2. Nuke DB (In-Memory Speed + Disk Persistence)
    cwist_error_t db_status = cwist_app_use_nuke_db(app, db_path, 5000);
    if (db_status.errtype != CWIST_ERR_INT16 || db_status.error.err_i16 != 0) {
        fprintf(stderr, "[ceversi] Nuke DB not available, falling back to standard SQLite for '%s'.\n", db_path);
        db_status = cwist_app_use_db(app, db_path);
        if (db_status.errtype != CWIST_ERR_INT16 || db_status.error.err_i16 != 0) {
            fprintf(stderr, "[ceversi] Unable to open database '%s'.\n", db_path);
            cwist_app_destroy(app);
            return 1;
        }
    }
    // ---------------------------------

    if (use_https) {
        cwist_app_use_https(app, "server.crt", "server.key");
    }

    cwist_db *db = cwist_app_get_db(app);
    if (!db) {
        fprintf(stderr, "[ceversi] Database handle unavailable, aborting.\n");
        cwist_app_destroy(app);
        return 1;
    }
    init_db(db); 

    pthread_t tid;
    pthread_create(&tid, NULL, cleanup_thread, db);
    pthread_detach(tid);

    // Explicit API Routes
    cwist_app_get(app, "/", root_handler);
    cwist_app_post(app, "/join", join_handler);
    cwist_app_post(app, "/leave", leave_handler);
    cwist_app_get(app, "/state", state_handler);
    cwist_app_post(app, "/move", move_handler);
    cwist_app_post(app, "/login", login_handler);
    cwist_app_post(app, "/register", register_handler);
    cwist_app_get(app, "/rankings", rankings_handler);
    cwist_app_get(app, "/user_info", user_info_handler);
    
    // Static files fallback
    cwist_app_static(app, "/static", "./public"); 

    printf("Starting %s Othello Server on port %d...\n", use_https ? "HTTPS" : "HTTP", port);
    
    int rc = cwist_app_listen(app, port);
    cwist_app_destroy(app);
    return rc;
}
