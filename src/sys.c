#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PHP_SCRIPT_SZ   512
#define TAIL_CMD_SZ     64
#define CMD_OUTPUT_SZ   2000
#define MSG_SZ          2048

#include "config.h"
#include "main.h"
#include "debug.h"
#include "report.h"
#include "sys.h"


typedef struct phpScriptS {
    char path[PHP_SCRIPT_SZ];
} phpScript;

static void* php_thread(void *pData) {
    // phpScript *scr = (phpScript*)pData;
    // posix_

    return NULL;
}

int sys_command(char *cmd) {
    int r;
    pthread_t th;
    phpScript scr;

    if(strcmp(cmd, "geos") == 0) {
        snprintf(scr.path, PHP_SCRIPT_SZ, "%s/load/gps_resources.php", SCRIPTS_PATH);
        r = pthread_create(&th, NULL, php_thread, &scr);
        if (r != 0) {
            logErr("GeoFences thread creation failed(%d): %s", r, strerror(abs(r)));
            return 1;
        }
    }
    return 0;
}

int sys_tail(int count, uint32_t chat) {
    static char cmd[TAIL_CMD_SZ];
    static char buf[CMD_OUTPUT_SZ];
    static char msg[MSG_SZ];
    if(count < 1) count = 1;
    snprintf(cmd, TAIL_CMD_SZ, "tail -n %d %s", count, PHP_LOG);
    logDbg("CMD: %s", cmd);
    FILE *p = popen(cmd, "r");
    if(p) {
        memset(buf, 0, CMD_OUTPUT_SZ);
        size_t sz = fread(buf, CMD_OUTPUT_SZ, 1, p);
        if(sz == 0 && feof(p)) {
            sz = strlen(buf);
        }
        logDbg("RET(%ld): %s", sz, buf);
        if(sz) {
            snprintf(msg, MSG_SZ, "✅ Tail(%d)\n```\n%s```", count, buf);
        } else {
            snprintf(msg, MSG_SZ, "🛑 ERR(%d): %m", errno);
        }
        pclose(p);
    } else {
        int err = errno;
        logErr("Tail error(%d): %s", err, strerror(err));
        snprintf(msg, MSG_SZ, "🛑 ERR(%d): %s", err, strerror(err));
    }
    send_report(chat, msg, 0);
    return 0;
}