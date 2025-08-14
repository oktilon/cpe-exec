#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define EXEC_PATH_SZ    512
#define TAIL_CMD_SZ     64
#define CMD_OUTPUT_SZ   2000
#define MSG_SZ          2048

#include "config.h"
#include "main.h"
#include "debug.h"
#include "report.h"
#include "sys.h"

#define ExecDoc     0x1

typedef struct execStructS {
    uint32_t chat;
    uint32_t respTo;
    uint8_t flag;
    char title[64];
    char path[EXEC_PATH_SZ];
    char doc[EXEC_PATH_SZ];
} execStruct;

typedef struct tgDataS {
    uint32_t chat;
} tgData;

static void* exec_thread(void *pData) {
    static char msg[MSG_SZ];
    execStruct *pStr = (execStruct*)pData;

    if(system(pStr->path) < 0) {
        snprintf(msg, MSG_SZ, "🛑 Execute %s error(%d): %m", pStr->title, errno);
        logErr(msg);
        send_report(pStr->chat, msg, pStr->respTo);
    }
    if (pStr->flag & ExecDoc) {
        send_document(pStr->chat, pStr->doc, pStr->title, pStr->respTo);
    } else {
        snprintf(msg, MSG_SZ, "✅ Execute %s done", pStr->title);
        send_report(pStr->chat, msg, pStr->respTo);
    }

    free(pData);
    return NULL;
}

static void do_pull(tgData *pTg) {
    static char msg[MSG_SZ];
    static char buf[CMD_OUTPUT_SZ];
    int r = chdir(GIT_PATH);
    if(r < 0) {
        snprintf(msg, MSG_SZ, "🛑 Pull cd error(%d): %m", errno);
        logErr(msg);
    } else {
        FILE *p = popen("git pull", "r");
        if(p) {
            memset(buf, 0, CMD_OUTPUT_SZ);
            size_t sz = fread(buf, CMD_OUTPUT_SZ, 1, p);
            if(sz == 0 && feof(p)) {
                sz = strlen(buf);
            }
            logDbg("RET(%ld): %s", sz, buf);
            if(sz) {
                snprintf(msg, MSG_SZ, "✅ Pull 🔹%ld\n```\n%s```", sz, buf);
            } else {
                snprintf(msg, MSG_SZ, "🛑 Pull error(%d): %m", errno);
            }
            pclose(p);
        } else {
            int err = errno;
            logErr("Pull error(%d): %s", err, strerror(err));
            snprintf(msg, MSG_SZ, "🛑 ERR(%d): %s", err, strerror(err));
        }
    }
    send_report(pTg->chat, msg, 0);
}

int sys_run_command(char *cmd, uint32_t chat) {
    int r;
    pthread_t th;
    execStruct *es = calloc(1, sizeof(execStruct));

    es->chat = chat;
    if(strcmp(cmd, "geos") == 0) {
        sprintf(es->title, "geos");
        snprintf(es->path, EXEC_PATH_SZ, "%s/load/gps_resources.php > %s/gps_resources.log", SCRIPTS_PATH, OUT_PATH);
        r = pthread_create(&th, NULL, exec_thread, es);
        if (r != 0) {
            logErr("GeoFences thread creation failed(%d): %s", r, strerror(abs(r)));
            return 0;
        }
    } else if(strcmp(cmd, "cars") == 0) {
        sprintf(es->title, "cars");
        snprintf(es->path, EXEC_PATH_SZ, "%s/load/gps_items.php > %s/gps_items.log", SCRIPTS_PATH, OUT_PATH);
        r = pthread_create(&th, NULL, exec_thread, es);
        if (r != 0) {
            logErr("Cars thread creation failed(%d): %s", r, strerror(abs(r)));
            return 0;
        }
    } else {
        logErr("Unknown command [%s]", cmd);
    }
    return (int)th;
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
            snprintf(msg, MSG_SZ, "✅ Tail 🔸%d\n```\n%s```", count, buf);
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

int sys_pull(uint32_t chat) {
    static tgData tg;
    pid_t child;

    tg.chat = chat;
    if((child = fork()) < 0) {
        logErr("Pull thread failed(%d): %m", errno);
        return 0;
    } else if (child == 0) {
        do_pull(&tg);
        exit(0);
    }
    return child;
}

int sys_export(uint32_t chat, uint32_t *orders, int count) {
    int r, i, cnt = 0;
    pthread_t th;
    char arg[EXEC_PATH_SZ] = {0};
    execStruct *es;

    snprintf(arg, EXEC_PATH_SZ, "%s/export.json", OUT_PATH);
    unlink(arg);
    snprintf(arg, EXEC_PATH_SZ, "%s/export.pretty.json", OUT_PATH);
    unlink(arg);

    for(i = 0; i < count; i++) {
        es = calloc(1, sizeof(execStruct));
        es->chat = chat;
        es->flag = ExecDoc;
        sprintf(es->title, "export_%u", orders[i]);
        snprintf(es->path, EXEC_PATH_SZ, "%s/export/export_orders.php %s/export -o %u", SCRIPTS_PATH, OUT_PATH, orders[i]);
        strcpy(es->doc, arg);
        r = pthread_create(&th, NULL, exec_thread, es);
        if (r != 0) {
            logErr("Export[%u] thread creation failed(%d): %s", orders[i], r, strerror(abs(r)));
        } else {
            cnt++;
        }
    }
    return cnt;
}

int sys_clear(uint32_t chat, uint32_t *orders, int count) {
    int r, cnt;
    pthread_t th;
    char arg[TAIL_CMD_SZ] = {0};
    execStruct *es = calloc(1, sizeof(execStruct));

    es->chat = chat;
    sprintf(es->title, "clear");
    cnt = snprintf(es->path, EXEC_PATH_SZ, "%s/check/m_clean_orders.php -o", SCRIPTS_PATH);
    for(r = 0; r < count; r++) {
        cnt += snprintf(arg, TAIL_CMD_SZ, " %u", orders[r]);
        strcat(es->path, arg);
    }
    cnt += snprintf(arg, TAIL_CMD_SZ, " > %s/clear.log", OUT_PATH);
    strcat(es->path, arg);

    r = pthread_create(&th, NULL, exec_thread, es);
    if (r != 0) {
        logErr("GeoFences thread creation failed(%d): %s", r, strerror(abs(r)));
        return 0;
    }
    return (int)th;
}