#include <systemd/sd-bus.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include "bus.h"
#include "sys.h"
#include "debug.h"
#include "main.h"
#include "config.h"

#define TABLE_FLAG  SD_BUS_VTABLE_UNPRIVILEGED
// #define TABLE_FLAG  0
// Local variables
static sd_bus       *bus;

// Definitions
static int bus_run_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError);
static int bus_tail_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError);
static int bus_pull_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError);
static int bus_clear_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError);
static int bus_export_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError);


/**
 * @brief Service interface items table
 */
static const sd_bus_vtable vTable[] = {
    SD_BUS_VTABLE_START (TABLE_FLAG),
    SD_BUS_METHOD_WITH_NAMES("run"
        , "su", SD_BUS_PARAM (command)
                SD_BUS_PARAM (chat)
        , "i",  SD_BUS_PARAM (pid)
        , bus_run_cb
        , TABLE_FLAG
    ),
    SD_BUS_METHOD_WITH_NAMES("tail"
        , "iu", SD_BUS_PARAM (count)
                SD_BUS_PARAM (chat)
        , "i",  SD_BUS_PARAM (err)
        , bus_tail_cb
        , TABLE_FLAG
    ),
    SD_BUS_METHOD_WITH_NAMES("clear"
        , "uau", SD_BUS_PARAM (chat)
                 SD_BUS_PARAM (orders)
        , "i",   SD_BUS_PARAM (pid)
        , bus_clear_cb
        , TABLE_FLAG
    ),
    SD_BUS_METHOD_WITH_NAMES("export"
        , "uau", SD_BUS_PARAM (chat)
                 SD_BUS_PARAM (orders)
        , "i",   SD_BUS_PARAM (pid)
        , bus_export_cb
        , TABLE_FLAG
    ),
    SD_BUS_METHOD_WITH_NAMES("pull"
        , "u", SD_BUS_PARAM (chat)
        , "i", SD_BUS_PARAM (pid)
        , bus_pull_cb
        , TABLE_FLAG
    ),
    SD_BUS_VTABLE_END
};

// static const char *bus_get_error (const sd_bus_error *e, int error) {
//     if (e) {
//         if (sd_bus_error_has_name (e, SD_BUS_ERROR_ACCESS_DENIED))
//             return "Access denied";

//         if (e->message)
//             return e->message;
//     }

//     return strerror (abs (error));
// }

int bus_init() {
    int r;
    const char* uniqueName;

    r = sd_bus_open_system (&bus);
    if(r < 0) {
        logErr("Failed to open system bus (%d): %s", r, strerror (-r));
        return r;
    }

    logDbg("Init vTable %s %s", DBUS_THIS_PATH, DBUS_THIS_NAME);
    r = sd_bus_add_object_vtable (bus,
                                NULL,
                                DBUS_THIS_PATH,
                                DBUS_THIS_NAME,
                                vTable,
                                NULL);
    if(r < 0) {
        logErr("Failed to issue interface (%d): %s", r, strerror (-r));
        return r;
    }

    r = sd_bus_request_name (bus, DBUS_THIS_NAME, 0);
    if(r < 0) {
        logErr("Failed to acquire service name (%d): %s", r, strerror(-r));
        return r;
    }

    r = sd_bus_get_unique_name (bus, &uniqueName);
    if(r < 0)
        logTrc("Unique name error (%d): %s", r, strerror (-r));
    else
        logTrc("Unique name: %s", uniqueName);

    r = sd_bus_get_fd (bus);
    logTrc("D-bus initialization is finished! (fd=%d)", r);
    return r;
}

void bus_deinit() {
    if (bus) {
        sd_bus_unref (bus);
    }
}

void bus_process() {
    int r;

    r = sd_bus_process (bus, NULL);
    if(r < 0) {
        logErr ("Failed to process bus (%d): %s", r, strerror (-r));
    }
}

static int bus_run_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError) {
    int r;
    uint32_t chat;
    char *cmd;
    r = sd_bus_message_read (m, "su", &cmd, &chat);
    r = sys_run_command(cmd, chat);
    return sd_bus_reply_method_return(m, "i", r);
}

static int bus_tail_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError) {
    int r, n;
    uint32_t chat;
    r = sd_bus_message_read (m, "iu", &n, &chat);
    if(!r) {
        logErr("Read params error(%d): %s", r, strerror(abs(r)));
    } else {
        r = sys_tail(n, chat);
    }
    return sd_bus_reply_method_return(m, "i", r);
}

static int bus_pull_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError) {
    int r;
    uint32_t chat;
    r = sd_bus_message_read (m, "u", &chat);
    if(!r) {
        logErr("Read params error(%d): %s", r, strerror(abs(r)));
    } else {
        r = sys_pull(chat);
    }
    return sd_bus_reply_method_return(m, "i", r);
}

int bus_read_array_u(sd_bus_message *m, uint32_t **ret) {
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "u");
    if (r < 0) return r;
    int cnt = 0;
    uint32_t it, *mem = (uint32_t*)malloc(sizeof(uint32_t));
    while (sd_bus_message_at_end(m, 0) == 0) {
        r = sd_bus_message_read (m, "u", &it);
        if(r < 0) {
            logWrn("Array item error(%d): %s", r, strerror(abs(r)));
        } else {
            cnt++;
            mem = (uint32_t*)realloc(mem, cnt * sizeof(uint32_t));
            if(mem)
                mem[cnt - 1] = it;
        }
    }
    if(ret) *ret = mem;
    sd_bus_message_exit_container (m);
    return cnt;
}

static int bus_export_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError) {
    int r, cnt;
    uint32_t chat, *orders = NULL;
    r = sd_bus_message_read (m, "u", &chat);
    if(r < 0) {
        logErr("Read param error(%d): %s", r, strerror(abs(r)));
    } else {
        cnt = bus_read_array_u(m, &orders);
        if(cnt < 0) {
            logErr("Read array error(%d): %s", r, strerror(abs(r)));
        } else if(cnt < 1 || !orders) {
            logErr("Invalid array");
        } else {
            r = sys_export(chat, orders, cnt);
        }
        free(orders);
    }
    return sd_bus_reply_method_return(m, "i", r);
}

static int bus_clear_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError) {
    int r, cnt;
    uint32_t chat, *orders = NULL;
    r = sd_bus_message_read (m, "u", &chat);
    if(r < 0) {
        logErr("Read param error(%d): %s", r, strerror(abs(r)));
    } else {
        cnt = bus_read_array_u(m, &orders);
        if(cnt < 0) {
            logErr("Read array error(%d): %s", r, strerror(abs(r)));
        } else if(cnt < 1 || !orders) {
            logErr("Invalid array");
        } else {
            r = sys_clear(chat, orders, cnt);
        }
        free(orders);
    }
    return sd_bus_reply_method_return(m, "i", r);
}