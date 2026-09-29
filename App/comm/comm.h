#ifndef COMM_H_
#define COMM_H_

#include "stdbool.h"
#include <cmsis_os.h>

extern osMutexId_t mutex_comm_tx;

bool comm_init(void);

#endif //  COMM_H_