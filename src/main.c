#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "storage.h"
#include "debug.h"
#include "bus.h"
#include "config.h"

/* global variables and constants */

pthread_mutex_t         gLogMutex;
int                     gLogFile    = 0;
int                     gLogConsole = false;
int                     gPrintHelp  = false;
static int              gLogLevel   = LOG_LEVEL_WARNING;    // Logging level
static int              gLogType    = LOG_TYPE_NORMAL;
static unsigned long    gLogFlush   = 0;



// Long command line options
const struct option longOptions[] = {
    {"verbose",         no_argument,        0,  'v'},
    {"quiet",           no_argument,        0,  'q'},
    {"extended-log",    no_argument,        0,  'x'},
    {"console",         no_argument,        0,  'c'},
    {"help",            no_argument,        0,  'h'}
};
const char *optionDesc[] = {
    "\tincreases log level +1",
    "\tminimal log level (ERR)",
    "extended log format",
    "\trun as a service (No timestamp in log)",
    "\tdisplay this help"
};
const char *shortOptions = "vqxch";

const char *logLevelHeader[] = {
    "\033[1;37mLOG\033[0m",  // LOG_LEVEL_ALWAYS    //
    "\033[1;31mERR\033[0m",  // LOG_LEVEL_ERROR     // q = quiet
    "\033[1;91mWRN\033[0m",  // LOG_LEVEL_WARNING   //   = default
    "\033[1;37mINF\033[0m",  // LOG_LEVEL_INFO      // v = verbose
    "\033[1;36mDBG\033[0m",  // LOG_LEVEL_DEBUG     // vv = verbose+
    "\033[1;33mTRC\033[0m"   // LOG_LEVEL_TRACE     // vvv = verbose++
};

const char *logLevelColor[] = {
    "\033[0;37m",  // LOG_LEVEL_ALWAYS  #D0CFCC
    "\033[0;31m",  // LOG_LEVEL_ERROR   #BC1B27
    "\033[0;91m",  // LOG_LEVEL_WARNING #F15E42
    "\033[0;37m",  // LOG_LEVEL_INFO    #D0CFCC
    "\033[0;36m",  // LOG_LEVEL_DEBUG   #2AA1B3
    "\033[0;33m"   // LOG_LEVEL_TRACE   #A2734C
};

void selfLogFunction(const char *file, int line, const char *fun, int logLevel, const char *fmt, ...) {
    if (logLevel <= gLogLevel) {
        int r, err;
        size_t sz;
        va_list ap;
        char *msg = NULL;
        struct tm t = {0};
        struct timeval tv;
        char tms[34] = {0};

        pthread_mutex_lock (&gLogMutex);

        if (logLevel < 0) logLevel = 0;
        if (logLevel > LOG_LEVEL_MAX) logLevel = LOG_LEVEL_MAX;

        if (gLogConsole) {
            gLogFile = fileno (stdout);
        } else {
            if (!gLogFile) {
                gLogFile = open (LOG_FILENAME, O_WRONLY | O_APPEND | O_CREAT, 0664);
            }
            if (!gLogFile) return;
        }

        gettimeofday(&tv, NULL);
        localtime_r(&tv.tv_sec, &t);
        sz = strftime(tms, sizeof(tms), "%F %T", &t); // %F => %Y-%m-%d,  %T => %H:%M:%S
        sprintf(tms + sz, ".%03ld: ", tv.tv_usec);

        va_start(ap, fmt);
        r = vasprintf(&msg, fmt, ap);
        err = errno;
        va_end(ap);

        if (r) {
            if (gLogType == LOG_TYPE_NORMAL) {
                dprintf(gLogFile, "%s[%s] %s %s%s%s\n"
                    , tms
                    , logLevelHeader[logLevel]
                    , fun
                    , logLevelColor[logLevel]
                    , msg
                    , COLOR_NONE);
            } else {
                dprintf(gLogFile, "%s[%s] %s %s%s%s [%s:%d]\n"
                    , tms
                    , logLevelHeader[logLevel]
                    , fun
                    , logLevelColor[logLevel]
                    , msg
                    , COLOR_NONE
                    , file
                    , line);
            }
        } else {
            dprintf(gLogFile, "%s[%s] %s %sMessage parse error(%d): %s%s\n"
                , tms
                , logLevelHeader[LOG_LEVEL_ERROR]
                , fun
                , logLevelColor[LOG_LEVEL_ERROR]
                , err
                , strerror(err)
                , COLOR_NONE);
        }

        if (gLogFlush <= tv.tv_sec) {
            fsync(gLogFile);
            gLogFlush = tv.tv_sec + 2;
        }

        if(msg)
            free(msg);

        pthread_mutex_unlock (&gLogMutex);
    }
}

void parse_options(int argc, char **argv) {
    int i;

    while ((i = getopt_long (argc, argv, shortOptions, longOptions, NULL)) != -1) {
        switch (i) {
            case 'v': // verbose
                gLogLevel++;
                if (gLogLevel > LOG_LEVEL_MAX) {
                    gLogLevel = LOG_LEVEL_MAX;
                }
                break;

            case 'q': // quiet
                gLogLevel = LOG_LEVEL_ERROR;
                break;

            case 'x': // extended-log
                gLogType = LOG_TYPE_EXTENDED;
                break;

            case 'c': // console
                gLogConsole = true;
                break;

            case 'h': // help
                gPrintHelp = true;
                break;

            default:
                break;
        }
    }
}

int main(int argc, char **argv) {

    parse_options(argc, argv);

    if (gPrintHelp) {
        int i, cnt;
        printf ("Usage: %s [-%s]\n", argv[0], shortOptions);
        cnt = sizeof(longOptions)/sizeof(struct option);
        for(i = 0; i < cnt; i++) {
            printf("\t-%c [--%s],\t%s\n", longOptions[i].val, longOptions[i].name, optionDesc[i]);
        }
        printf ("Log levels:\n");
        for(i = 0; i <= LOG_LEVEL_MAX; i++) {
            printf("\t%d = %s%s\n", i, logLevelHeader[i], i == gLogLevel ? " (default)" : "");
        }
        return 0;
    }

    log("Run %s with %d args", argv[0], argc);
    // if(!storage_init(argc, argv)) {
    //     logErr("Error exit");
    //     return 1;
    // }

    if(bus_init() < 0) {
        logErr("Bus error");
        return 1;
    }

    while(true) {
        bus_process();
        usleep(10000);
    }

    return 0;
}

