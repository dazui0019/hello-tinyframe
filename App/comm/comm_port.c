#include "main.h"
#include "usart.h"
#include "dma.h"
#include "comm.h"
#include "comm_port.h"
#include "cmsis_os.h"
#include "lwrb/lwrb.h"

lwrb_t* uart_rb = NULL;

uint8_t uartRxBuffer[UART_RX_BUFFER_SIZE];

__IO uint32_t uwNbReceivedChars;

osSemaphoreId_t rx_sem = NULL;

bool comm_port_init(lwrb_t* rx_rb, osSemaphoreId_t rx_notify_sem)
{
    uwNbReceivedChars = 0;

    rx_sem = rx_notify_sem;
    uart_rb = rx_rb;

    HAL_UARTEx_ReceiveToIdle_DMA(&huart5, uartRxBuffer, UART_RX_BUFFER_SIZE);

    return true;
}

void UART5_RxEventCallback(uint16_t Size)
{
    static uint8_t old_pos = 0;

    if (Size != old_pos)
    {
        if (Size > old_pos)
        {
            uwNbReceivedChars = Size - old_pos;

            lwrb_write(uart_rb, uartRxBuffer+old_pos, uwNbReceivedChars);
        }
        else
        {
            /* Size <= old_pos: DMA缓冲区被写满一轮, Size < old_pos 表示有一部分覆盖到DMA缓冲区开头 */
            uwNbReceivedChars = UART_RX_BUFFER_SIZE - old_pos;

            lwrb_write(uart_rb, uartRxBuffer+old_pos, uwNbReceivedChars);

            /* 处理溢出部分 */
            if (Size > 0)
            {
                lwrb_write(uart_rb, uartRxBuffer, Size);

                uwNbReceivedChars += Size;
            }
        }
        osSemaphoreRelease(rx_sem);
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
