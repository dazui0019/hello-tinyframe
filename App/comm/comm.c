#include "comm.h"
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include "gpio.h"
#include "TinyFrame.h"

static void comm_task(void *argument);

TinyFrame tf;
TF_Peer peer_bit = TF_MASTER;

osThreadId_t comm_task_handle;
const osThreadAttr_t comm_task_attr = {
  .name = "commTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osMutexId_t mutex_comm_tx;
const osMutexAttr_t tx_mutex_attr = {
    .name = "txMutex",
    .attr_bits = osMutexPrioInherit,
    .cb_mem = NULL,
    .cb_size = 0
};

bool comm_init(void)
{
    if (!TF_InitStatic(&tf, TF_MASTER)) {
        return false;
    }

    mutex_comm_tx = osMutexNew(&tx_mutex_attr);
    if(mutex_comm_tx == NULL){
        return false;
    }

    comm_task_handle = osThreadNew(comm_task, NULL, &comm_task_attr);

    return comm_task_handle != NULL;
}

static void comm_task(void *argument)
{
    for (;;) {
        osDelay(100);
    }
}
