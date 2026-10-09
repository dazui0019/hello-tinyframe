#include "main.h"
#include "usart.h"
#include "dma.h"
#include "comm.h"
#include "comm_port.h"
#include "cmsis_os.h"
#include "lwrb/lwrb.h"

uint8_t lwrbBuffer[RING_BUFFER_SIZE] = {0};
lwrb_t uart_rb;

uint8_t uartRxBuffer[UART_RX_BUFFER_SIZE];

__IO uint32_t uwNbReceivedChars;

osMessageQueueId_t uartMsgQueue;

osThreadId_t UartTaskHandle;
const osThreadAttr_t thread_uart_attr = {
  .name = "thread_uart",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

static uint8_t rbReadBuffer[UART_RX_BUFFER_SIZE];

static void UartTask(void *argument)
{
    (void)argument;

    UART_MSGQUEUE_OBJ_t event;

    for (;;)
    {
        if (osMessageQueueGet(uartMsgQueue, &event,
                              NULL, osWaitForever) != osOK)
        {
            continue;
        }

        size_t count;
        for (;;)
        {
            count = lwrb_read(&uart_rb, rbReadBuffer, sizeof(rbReadBuffer));
            if (count == 0)
            {
                break;
            }
        }
    }
}

bool comm_port_init(void)
{
    uwNbReceivedChars = 0;

    lwrb_init(&uart_rb, lwrbBuffer, sizeof(lwrbBuffer));

    uartMsgQueue = osMessageQueueNew(UART_MSGQUEUE_OBJECTS, sizeof(UART_MSGQUEUE_OBJ_t), NULL);
    if (uartMsgQueue == NULL)
    {
        return false;
    }

    UartTaskHandle = osThreadNew(UartTask, NULL, &thread_uart_attr);
    if (UartTaskHandle == NULL)
    {
        return false;
    }

    HAL_UARTEx_ReceiveToIdle_DMA(&huart5, uartRxBuffer, UART_RX_BUFFER_SIZE);

    return true;
}

void UART5_RxEventCallback(uint16_t Size)
{
    static uint8_t old_pos = 0;
    UART_MSGQUEUE_OBJ_t event;

    if (Size != old_pos)
    {
        if (Size > old_pos)
        {
            uwNbReceivedChars = Size - old_pos;

            event.buffer = uartRxBuffer+old_pos;
            event.len = uwNbReceivedChars;
            osMessageQueuePut(uartMsgQueue, &event, 0U, 0U);
        }
        else
        {
            /* Size <= old_pos: DMA缓冲区被写满一轮了, Size < old_pos 表示有一部分覆盖到DMA缓冲区开头 */
            uwNbReceivedChars = UART_RX_BUFFER_SIZE - old_pos;

            event.buffer = uartRxBuffer+old_pos;
            event.len = uwNbReceivedChars;
            osMessageQueuePut(uartMsgQueue, &event, 0U, 0U);

            /* 处理溢出部分 */
            if (Size > 0)
            {
                event.buffer = uartRxBuffer;
                event.len = Size;
                osMessageQueuePut(uartMsgQueue, &event, 0U, 0U);
                uwNbReceivedChars += Size;
            }
        }
    }
    old_pos = Size;
}

void UART5_IRQHandler(void)
{
    uint16_t Size;
    if (__HAL_UART_GET_FLAG(&huart5, UART_FLAG_IDLE))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart5);

        Size = UART_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart5.hdmarx);
        UART5_RxEventCallback(Size);
    }
    else if (__HAL_UART_GET_FLAG(&huart5, UART_FLAG_RXNE))
    {
        __HAL_UART_CLEAR_FLAG(&huart5, UART_FLAG_RXNE);
    }
    else
    {
        __HAL_UART_CLEAR_PEFLAG(&huart5);
    }
}

void DMA1_Stream0_IRQHandler(void)
{
    if (__HAL_DMA_GET_FLAG(huart5.hdmarx, DMA_FLAG_HTIF0_4))
    {
        __HAL_DMA_CLEAR_FLAG(huart5.hdmarx, DMA_FLAG_HTIF0_4);
        UART5_RxEventCallback(UART_RX_BUFFER_SIZE / 2);
    }
    else if (__HAL_DMA_GET_FLAG(huart5.hdmarx, DMA_FLAG_TCIF0_4))
    {
        __HAL_DMA_CLEAR_FLAG(huart5.hdmarx, DMA_FLAG_TCIF0_4);
        UART5_RxEventCallback(UART_RX_BUFFER_SIZE);
    }
    else
    {
        ;
    }
}
