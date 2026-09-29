#include "main.h"
#include "gpio.h"
#include "cmsis_os.h"

void StartLedTask(void *argument)
{
  for(;;)
  {
    HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
    osDelay(100);
  }
}
