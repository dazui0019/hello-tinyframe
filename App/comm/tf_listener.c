#include "TinyFrame.h"
#include "tf_listener.h"
#include "comm_protocol.h"
#include "led/led.h"

static TF_Result echo_listener(TinyFrame *tf, TF_Msg *msg)
{
    /* 使用收到的载荷作为回复，沿用请求 ID */
    if (!TF_Respond(tf, msg)) {
        /* 在这里记录回复发送失败 */
    }

    return TF_STAY;
}

/*
 * LED 控制报文：
 * 请求：TYPE=02，DATA 每项为 [LED编号, 目标状态]，LEN=项数*2。
 *       编号范围为 0～LED_COUNT-1，状态为 00=关、01=开；最多 LED_COUNT 项。
 * 响应：TYPE=02，沿用请求 ID，DATA 每项为 [LED编号, 原始目标状态, 结果码]，
 *       LEN=项数*3，与请求顺序一一对应，整批处理后只回复一次。
 * 结果：00=已执行 GPIO 写入，01=执行失败，02=参数错误；03=未执行（预留）。
 *       单项参数错误不影响其他合法项执行；重复编号按请求顺序执行。
 */
static TF_Result led_listener(TinyFrame *tf, TF_Msg *msg)
{
    /* Only the RX task invokes this listener. Keep the buffer off its stack. */
    static uint8_t results[LED_COUNT*3];
    TF_Msg reply = *msg;

    /* Malformed batches execute nothing and return a one-byte error. */
    if (msg->len == 0 || (msg->len % 2U) != 0 ||
        (msg->len / 2U) > LED_COUNT)
    {
        results[0] = COMM_RESULT_INVALID;
        reply.len = 1;
    }
    else
    {
        uint32_t out = 0;
        for (uint32_t i = 0; i < msg->len; i += 2U)
        {
            uint8_t index = msg->data[i];
            uint8_t state = msg->data[i + 1U];
            uint8_t result;
            if (index >= LED_COUNT || state > 1U)
            {
                result = COMM_RESULT_INVALID;
            }
            else
            {
                result = led_ctrl(index, state != 0U) ?
                    COMM_RESULT_OK : COMM_RESULT_FAILED;
            }
            results[out++] = index;
            results[out++] = state;
            results[out++] = result;
        }
        reply.len = (TF_LEN)out;
    }
    reply.data = results;
    if (!TF_Respond(tf, &reply))
    {
        TF_Error("LED reply could not be sent");
    }
    return TF_STAY;
}

bool listener_register(TinyFrame *tf)
{
    if (!TF_AddTypeListener(tf, COMM_TYPE_ECHO, echo_listener))
    {
        return false;
    }

    if (!TF_AddTypeListener(tf, COMM_TYPE_LED_CTRL, led_listener))
    {
        return false;
    }

    return true;
}
