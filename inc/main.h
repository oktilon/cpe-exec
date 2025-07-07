#pragma once

typedef struct adminCommandStruct {
    int id;
    int cmd;
    char *obj;
    int flags;
    int usr;
    int dt;
    int dt_exec;
} adminCommand;


