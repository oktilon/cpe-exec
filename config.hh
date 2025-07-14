#pragma once

#define VER_MAJ @version_major@
#define VER_MIN @version_minor@
#define VER_REV @version_rev@
#define VER_FUN(s)    #s
#define VER_FUN2(s)   VER_FUN(s)
#define VER_STR VER_FUN2(VAR_MAJ) "." VER_FUN2(VAR_MIN) "." VER_FUN2(VAR_REV)

#define SQL_SERV    "@db_serv@"
#define SQL_PORT    @db_port@
#define SQL_BASE    "@db_base@"
#define SQL_USER    "@db_user@"
#define SQL_PASS    "@db_pass@"
#define API_KEY     "@tg_key@"
#define ADMIN_CHAT  @tg_chat@

// Bus
#define DBUS_THIS_NAME          "@bus_srv_name@"
#define DBUS_THIS_PATH          "@bus_srv_path@"

// File locations
#define LOG_FILENAME        "@log_file@"
#define SCRIPTS_PATH        "@path@"
#define GIT_PATH            "@git_path@"
#define OUT_PATH            "@out_path@"
#define PHP_LOG             "@php_log@"