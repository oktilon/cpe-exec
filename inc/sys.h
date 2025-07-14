#pragma once
#include <stdint.h>

int sys_run_command(char *cmd, uint32_t chat);
int sys_tail(int count, uint32_t chat);
int sys_pull(uint32_t chat);
int sys_export(uint32_t chat, uint32_t *orders, int cnt);
int sys_clear(uint32_t chat, uint32_t *orders, int cnt);