#ifndef COMM_PORT_H_
#define COMM_PORT_H_

#include <stdint.h>
#include "lwrb/lwrb.h"

bool comm_port_init(lwrb_t* rx_rb, osSemaphoreId_t rx_notify_sem);

#define UART_RX_BUFFER_SIZE   (128)
#define RING_BUFFER_SIZE (2*UART_RX_BUFFER_SIZE)

#endif // !COMM_PORT_H_