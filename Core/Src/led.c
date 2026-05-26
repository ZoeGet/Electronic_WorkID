#include "led.h"
#include "main.h"

static uint32_t lastToggleTime = 0;
static const uint32_t LED_INTERVAL = 500;

void LED_Init(void)
{
    lastToggleTime = HAL_GetTick();
}

void LED_Process(void)
{
    uint32_t currentTime = HAL_GetTick();

    if (currentTime - lastToggleTime >= LED_INTERVAL)
    {
        lastToggleTime = currentTime;
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    }
}
