#include "main.h"
#include "usart.h"
#include "dma.h"
#include "comm.h"
#include "comm_port.h"
#include "cmsis_os.h"
#include "lwrb/lwrb.h"

typedef struct {
    volatile uint32_t offered_bytes;     // DMA 提交的字节数
    volatile uint32_t written_bytes;     // 实际写入 FIFO 的字节数
    volatile uint32_t dropped_bytes;     // FIFO 空间不足丢弃的字节数
    volatile uint32_t notify_failures;   // 队列通知失败次数
    volatile uint32_t fifo_high_water;   // FIFO 最高占用量

    volatile uint32_t read_bytes;        // 任务实际读取的字节数
    volatile uint32_t last_read_pos;     // 最近一次成功读取前的读位置，lwrbBuffer 下标
    volatile uint32_t verified_bytes;    // 已完成检查的字节数，包含匹配和不匹配
    volatile uint32_t mismatch_count;    // 数据顺序不符合预期的次数
    volatile uint32_t queue_put_cycles;   // 最近一次 put 前的 DWT 周期计数
    volatile uint32_t queue_get_cycles;   // 最近一次 get 成功后的 DWT 周期计数
} uart_test_stats_t;

static uart_test_stats_t rx_stats;

uint8_t lwrbBuffer[RING_BUFFER_SIZE] = {0};
lwrb_t uart_rb;

uint8_t aRXBufferUser[UART_RX_BUFFER_SIZE];

__IO uint32_t     uwNbReceivedChars;

osMessageQueueId_t comm_MsgQueue;

osThreadId_t UartTaskHandle;
const osThreadAttr_t thread_uart_attr = {
  .name = "thread_uart",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

static uint8_t test_read_buff[UART_RX_BUFFER_SIZE];

void UartTask(void *argument)
{
    (void)argument;

    UART_MSGQUEUE_OBJ_t event;
    uint8_t expected = 0; // 测试从 0x00 开始

    for (;;) {
        if (osMessageQueueGet(comm_MsgQueue, &event,
                              NULL, osWaitForever) != osOK) {
            continue;
        }

        rx_stats.queue_get_cycles = DWT->CYCCNT;

        size_t count;
        for (;;) {
            const uint8_t *read_address = lwrb_get_linear_block_read_address(&uart_rb);
            count = lwrb_read(&uart_rb, test_read_buff, sizeof(test_read_buff));
            if (count == 0) {
                break;
            }
            rx_stats.last_read_pos = (uint32_t)(read_address - lwrbBuffer);
            rx_stats.read_bytes += count;

            for (size_t i = 0; i < count; i++) {
                uint8_t actual = test_read_buff[i];

                if (actual != expected) {
                    rx_stats.mismatch_count++;
                }

                // 跟随实际值重新同步，避免一次异常导致后面全报错
                expected = (uint8_t)(actual + 1U);
                rx_stats.verified_bytes++;
            }
        }
    }
}

bool comm_port_init(void)
{
    uwNbReceivedChars = 0;

    // 启用 DWT 周期计数器，不重置已有计数。
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();

    lwrb_init(&uart_rb, lwrbBuffer, sizeof(lwrbBuffer));

    comm_MsgQueue = osMessageQueueNew(UART_MSGQUEUE_OBJECTS, sizeof(UART_MSGQUEUE_OBJ_t), NULL);
    if (comm_MsgQueue == NULL) {
        return false;
    }

    UartTaskHandle = osThreadNew(UartTask, NULL, &thread_uart_attr);
    if (UartTaskHandle == NULL) {
        return false;
    }

    HAL_UARTEx_ReceiveToIdle_DMA(&huart5, aRXBufferUser, UART_RX_BUFFER_SIZE);

    return true;
}

/* 由 UART/DMA 中断调用；两个中断不能互相抢占此函数 */
static void rx_submit_from_isr(const uint8_t *data, size_t len)
{
    size_t written = lwrb_write(&uart_rb, data, len);

    rx_stats.offered_bytes += len;
    rx_stats.written_bytes += written;
    rx_stats.dropped_bytes += len - written;

    size_t used = lwrb_get_full(&uart_rb);
    if (used > rx_stats.fifo_high_water) {
        rx_stats.fifo_high_water = used;
    }

    if (written > 0) {
        UART_MSGQUEUE_OBJ_t event = {0};

        // 仅用于唤醒，不再用 event.len 决定读取长度
        rx_stats.queue_put_cycles = DWT->CYCCNT;
        if (osMessageQueuePut(comm_MsgQueue, &event, 0U, 0U) != osOK) {
            rx_stats.notify_failures++;
        }
    }
}

static void uart_rx_event(uint16_t Size)
{
    static uint8_t old_pos = 0;

    if (Size != old_pos)
    {
        if (Size > old_pos)
        {
            uwNbReceivedChars = Size - old_pos;
            rx_submit_from_isr(aRXBufferUser+old_pos, uwNbReceivedChars);
        }
        else
        {
            uwNbReceivedChars = UART_RX_BUFFER_SIZE - old_pos;
            rx_submit_from_isr(aRXBufferUser+old_pos, uwNbReceivedChars);
            if (Size > 0)
            {
                rx_submit_from_isr(aRXBufferUser, Size);
                uwNbReceivedChars += Size;
            }
        }
    }
    old_pos = Size;
}

void UART5_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart5, UART_FLAG_IDLE))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart5);
        uart_rx_event(UART_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart5.hdmarx));
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
        uart_rx_event(UART_RX_BUFFER_SIZE / 2);
    }
    else if (__HAL_DMA_GET_FLAG(huart5.hdmarx, DMA_FLAG_TCIF0_4))
    {
        __HAL_DMA_CLEAR_FLAG(huart5.hdmarx, DMA_FLAG_TCIF0_4);
        uart_rx_event(UART_RX_BUFFER_SIZE);
    }
    else
    {
        ;
    }
}
