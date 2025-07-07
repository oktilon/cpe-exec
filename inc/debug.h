#pragma once
#include <errno.h>

/********************************
 *           LOGGING            *
*********************************/
// UI service log levels
#define LOG_LEVEL_ALWAYS    0
#define LOG_LEVEL_ERROR     1
#define LOG_LEVEL_WARNING   2
#define LOG_LEVEL_INFO      3
#define LOG_LEVEL_DEBUG     4
#define LOG_LEVEL_TRACE     5
#define LOG_LEVEL_MAX       LOG_LEVEL_TRACE

#define LOG_TYPE_NORMAL     0
#define LOG_TYPE_EXTENDED   1

void selfLogFunction (const char *file, int line, const char *func, int lvl, const char* fmt, ...);

// Log always
#define log(FMT, ...)    selfLogFunction ((const char *)(__FILE__), __LINE__, (const char *)(__PRETTY_FUNCTION__), LOG_LEVEL_ALWAYS,  FMT __VA_OPT__ (,) __VA_ARGS__)
// Log ERROR level
#define logErr(FMT, ...) selfLogFunction ((const char *)(__FILE__), __LINE__, (const char *)(__PRETTY_FUNCTION__), LOG_LEVEL_ERROR,   FMT __VA_OPT__ (,) __VA_ARGS__)
// Log WARNING level
#define logWrn(FMT, ...) selfLogFunction ((const char *)(__FILE__), __LINE__, (const char *)(__PRETTY_FUNCTION__), LOG_LEVEL_WARNING, FMT __VA_OPT__ (,) __VA_ARGS__)
// Log INFO level
#define logInf(FMT, ...) selfLogFunction ((const char *)(__FILE__), __LINE__, (const char *)(__PRETTY_FUNCTION__), LOG_LEVEL_INFO,    FMT __VA_OPT__ (,) __VA_ARGS__)
// Log DEBUG level
#define logDbg(FMT, ...) selfLogFunction ((const char *)(__FILE__), __LINE__, (const char *)(__PRETTY_FUNCTION__), LOG_LEVEL_DEBUG,   FMT __VA_OPT__ (,) __VA_ARGS__)
// Log TRACE level
#define logTrc(FMT, ...) selfLogFunction ((const char *)(__FILE__), __LINE__, (const char *)(__PRETTY_FUNCTION__), LOG_LEVEL_TRACE,   FMT __VA_OPT__ (,) __VA_ARGS__)

#define errorExit(CODE, FMT, ...) do { selfLogFunction ((const char *)(__FILE__), __LINE__, (const char *)(__PRETTY_FUNCTION__), LOG_LEVEL_ERROR,   FMT __VA_OPT__ (,) __VA_ARGS__); exit (-CODE); } while (0);

/********************************
 *            COLORS            *
*********************************/
#define CSI                 "\033["
#define SGR(n)              CSI n "m"
#define SGR_BOLD(n)         CSI "1;" n "m"

#define SGR_CODE_BLACK          "0"
#define SGR_CODE_RED            "1"
#define SGR_CODE_GREEN          "2"
#define SGR_CODE_YELLOW         "3"
#define SGR_CODE_BLUE           "4"
#define SGR_CODE_MAGENTA        "5"
#define SGR_CODE_CYAN           "6"
#define SGR_CODE_WHITE          "7"

#define SGR_FORE(clr)           "3" clr
#define SGR_BACK(clr)           "4" clr
#define SGR_FORE_BRIGHT(clr)    "9" clr
#define SGR_BACK_BRIGHT(clr)    "10" clr

#define COLOR_NONE          SGR("0")