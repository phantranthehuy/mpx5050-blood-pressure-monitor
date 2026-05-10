#include "stm32f1xx_hal.h"

extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart1;

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
