#pragma once
#include "main.h"

int storage_init(int argc, char **argv);
int storage_get_command(int id, adminCommand *pCmd);
void storage_close();