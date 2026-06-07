#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void scheduler_init(void);
void scheduler_run(void);

#ifdef __cplusplus
}
#endif

#endif
