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


/**
 * @brief Service interface items table
 */
static const sd_bus_vtable vTable[] = {
    SD_BUS_VTABLE_START (SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_NAMES("run"
        , "su", SD_BUS_PARAM (command)
                SD_BUS_PARAM (chat)
        , "i", SD_BUS_PARAM (err)
        , bus_run_cb
        , SD_BUS_VTABLE_UNPRIVILEGED
    ),
    SD_BUS_METHOD_WITH_NAMES("tail"
        , "iu", SD_BUS_PARAM (count)
                SD_BUS_PARAM (chat)
        , "i", SD_BUS_PARAM (err)
        , bus_tail_cb
        , SD_BUS_VTABLE_UNPRIVILEGED
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

int bus_show_message(int clk, int sec) {
    // sd_bus_message *ans;
    // sd_bus_error err = SD_BUS_ERROR_NULL;
    // int r, bOk = 0;
    int bOk = 0;

    /*r = sd_bus_call_method (bus
        , DBUS_UI_NAME
        , DBUS_UI_PATH
        , DBUS_UI_INTERFACE
        , "show"
        , &err
        , &ans
        , "ii"
        , clk
        , sec
    );

    if(r < 0) {
        logErr("Call SHOW error: %s", bus_get_error(&err, r));
    } else {
        r = sd_bus_message_read_basic(ans, 'b', &bOk);
        if (r < 0) {
            logWrn("Get reply code error(%d): %s", r, strerror(-r));
        } else {
            logDbg("SHOW call returned: %s", bOk ? "True" : "False");
        }
    }*/
    return bOk;
}

int bus_hide_message() {
    // sd_bus_message *ans;
    // sd_bus_error err = SD_BUS_ERROR_NULL;
    // int r, bOk = 0;
    int bOk = 0;

    /*r = sd_bus_call_method (bus
        , DBUS_UI_NAME
        , DBUS_UI_PATH
        , DBUS_UI_INTERFACE
        , "hide"
        , &err
        , &ans
        , ""
    );

    if(r < 0) {
        logErr("Call SHOW error: %s", bus_get_error(&err, r));
    } else {
        r = sd_bus_message_read_basic(ans, 'b', &bOk);
        if (r < 0) {
            logWrn("Get reply code error(%d): %s", r, strerror(-r));
        } else {
            logDbg("SHOW call returned: %s", bOk ? "True" : "False");
        }
    }*/
    return bOk;
}

static int bus_run_cb (sd_bus_message *m, void *userdata, sd_bus_error *retError) {
    int r;
    char *cmd;
    r = sd_bus_message_read (m, "s", &cmd);
    r = sys_command(cmd);
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