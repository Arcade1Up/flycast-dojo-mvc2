#ifndef EMUTICKS_H
#define EMUTICKS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void TicksInitialize();
int64_t TicksGetUSECs();
unsigned int TicksGetMSECs();
unsigned int TicksGetSECs();

#ifdef __cplusplus
}
#endif
#endif

