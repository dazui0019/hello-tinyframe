#ifndef LED_H_
#define LED_H_

#include "gpio.h"
#include <stdbool.h>
#include <stdint.h>

#define LED_COUNT 2U

#define LED_VALID_LEVEL     GPIO_PIN_RESET
#define LED_INVALID_LEVEL   GPIO_PIN_SET

bool led_ctrl(uint8_t index, bool state);

#endif // !LED_H_
