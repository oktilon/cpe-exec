#include <stdio.h>
#include <mysql.h>
#include <errno.h>

#include "main.h"
#include "debug.h"
#include "config.h"

MYSQL mySql = {0};

int storage_init(int argc, char **argv) {
    if (mysql_library_init(argc, argv, NULL)) {
        logErr("mysql_library_init error");
        return -1;
    }

    if(!mysql_init(&mySql)) {
        logErr("mysql_init error");
        return -2;
    }

    if(!mysql_real_connect(&mySql, SQL_SERV, SQL_USER, SQL_PASS, SQL_BASE, SQL_PORT, NULL, 0)) {
        logErr("Connect error: %s", mysql_error(&mySql));
        return -3;
    }

    return 0;
}

int storage_get_command(int id, adminCommand *pCmd) {
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[1024];
    if(!id || !pCmd) return -EINVAL;

    snprintf(sql, 1024, "SELECT cmd, obj, flags, usr, dt, dt_exec FROM adm_cmd WHERE id=%d", id);

    if(mysql_query(&mySql, sql)) {
        logErr("Query error: %s", mysql_error(&mySql));
        return -1;
    }

    if((res = mysql_store_result(&mySql))) {
        logDbg("Result has %d fields", mysql_num_fields(res));
        while((row = mysql_fetch_row(res))) {
            int i = atoi(row[0]);
            logDbg("Command %d has cmd=%d [%s], obj=%s", id, i, row[0], row[1]);
        }
        mysql_free_result(res);
    }

    return 0;
}

void storage_close() {
    mysql_close(&mySql);
    mysql_library_end();
}

