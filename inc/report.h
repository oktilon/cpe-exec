#include <stdint.h>

int send_report(uint32_t chat, char *msg, uint32_t responseTo);
int send_document(uint32_t chat, char *path, char *caption, uint32_t responseTo);