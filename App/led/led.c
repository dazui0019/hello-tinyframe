#include "main.h"
#include <stdbool.h>
#include <stdint.h>
#include "gpio.h"
#include "led.h"

typedef struct
{
    GPIO_TypeDef* GPIOx;
    uint16_t Pin;
}ledPin_t;

static const ledPin_t led_pin_map[LED_COUNT] = {
    {.GPIOx = LED0_GPIO_Port, .Pin = LED0_Pin},
    {.GPIOx = LED1_GPIO_Port, .Pin = LED1_Pin},
};

bool led_ctrl(uint8_t index, bool state)
{
    if (index >= LED_COUNT)
    {
        return false;
    }
    GPIO_TypeDef *led_GPIOx = led_pin_map[index].GPIOx;
    uint16_t led_Pin = led_pin_map[index].Pin;
    GPIO_PinState led_state = state ? LED_VALID_LEVEL : LED_INVALID_LEVEL;

    if (led_GPIOx == NULL || led_Pin == 0)
    {
        return false;
    }
    HAL_GPIO_WritePin(led_GPIOx, led_Pin, led_state);
    return true;
}
