#include "comm.h"
#include "main.h"
#include "cmsis_os.h"
#include "TinyFrame.h"
#include "comm_port.h"
#include "lwrb/lwrb.h"
#include "tf_listener.h"

static TinyFrame tf;

static osThreadId_t tfRcvTaskHandle;
const osThreadAttr_t tfRcvTask_attributes = {
  .name = "tfRcvTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osMutexId_t tfTxMutex;
const osMutexAttr_t tfTxMutex_attributes = {
    .name = "tfTxMutex",
    .attr_bits = osMutexPrioInherit,
    .cb_mem = NULL,
    .cb_size = 0
};

static osSemaphoreId_t uartRxSemaphore;

static uint8_t rbReadBuffer[UART_RX_BUFFER_SIZE];

static uint8_t lwrbBuffer[RING_BUFFER_SIZE] = {0};
static lwrb_t rx_rb;

static void tfRcvTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        if (osSemaphoreAcquire(uartRxSemaphore, osWaitForever) != osOK)
        {
            continue;
        }

        size_t count = 0;
        for (;;)
        {
            count = lwrb_read(&rx_rb, rbReadBuffer, sizeof(rbReadBuffer));
            if (count == 0)
            {
                break;
            }
            TF_Accept(&tf, rbReadBuffer, count);
        }
    }
}

bool comm_init(void)
{
    lwrb_init(&rx_rb, lwrbBuffer, sizeof(lwrbBuffer));

    uartRxSemaphore = osSemaphoreNew(1U, 0U, NULL);

    if (uartRxSemaphore == NULL)
    {
        return false;
    }
    tfTxMutex = osMutexNew(&tfTxMutex_attributes);
    if (tfTxMutex == NULL)
    {
        return false;
    }

    if (!TF_InitStatic(&tf, TF_MASTER))
    {
        return false;
    }
    listener_register(&tf);

    comm_port_init(&rx_rb, uartRxSemaphore);

    tfRcvTaskHandle = osThreadNew(tfRcvTask, NULL, &tfRcvTask_attributes);
    if (tfRcvTaskHandle == NULL)
    {
        return false;
    }

    return true;
}
