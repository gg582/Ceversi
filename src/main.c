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

#define PORT 31744

void *cleanup_thread(void *arg) {
    cwist_db *db = (cwist_db *)arg;
    while(1) {
        sleep(60);
        cleanup_stale_rooms(db);
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

    cwist_app *app = cwist_app_create();
    if (!app) {
        fprintf(stderr, "Failed to create cwist app\n");
        return 1;
    }

    // --- Smart Features Integration ---
    // 1. Conservative Memory Limit for Static Files (Fail-safe)
    cwist_app_set_max_memspace(app, CWIST_MIB(128)); 

    // 2. Nuke DB (In-Memory Speed + Disk Persistence)
    if (cwist_nuke_init("ceversi_nuke.db", 5000) == 0) {
        printf("Nuke DB Initialized.\n");
        // Hack: We want the app to use the Nuke DB handle.
        // Since cwist_app_use_db opens a new file, we simulate it.
        // We create the wrapper manually.
        app->db = malloc(sizeof(cwist_db));
        app->db->conn = cwist_nuke_get_db(); // Use the shared in-memory handle
        app->db_path = strdup(":memory:");   // It is effectively memory
    } else {
        // Fallback
        cwist_app_use_db(app, "othello.db");
    }
    // ---------------------------------

    if (use_https) {
        cwist_app_use_https(app, "server.crt", "server.key");
    }

    cwist_db *db = cwist_app_get_db(app);
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
    
    return cwist_app_listen(app, port);
}
