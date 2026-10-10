#ifndef COMM_H_
#define COMM_H_

#include "stdbool.h"
#include <cmsis_os.h>

typedef struct {
  uint8_t* buffer;
  uint8_t len;
} UART_MSGQUEUE_OBJ_t;
#define UART_MSGQUEUE_OBJECTS 16

extern osMutexId_t tfTxMutex;

bool comm_init(void);

#endif //  COMM_H_
