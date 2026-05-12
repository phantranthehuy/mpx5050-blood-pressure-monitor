#include "stm32f1xx_hal.h"

#include "board_config.h"
#include "ads1115.h"
#include "pressure.h"
#include "bp_fsm.h"
#include "uart_proto.h"
#include "led_hmi.h"

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

static Ads1115_Handle ads;
static volatile uint8_t g_tick_100hz = 0;

static void Error_Handler(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = LED_RED_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_RED_PORT, &g);
    while (1) {
        HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN);
        HAL_Delay(120);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oc = {0};
    RCC_ClkInitTypeDef cc = {0};

    oc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oc.HSEState = RCC_HSE_ON;
    oc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    oc.HSIState = RCC_HSI_ON;
    oc.PLL.PLLState = RCC_PLL_ON;
    oc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    oc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&oc) != HAL_OK)
        Error_Handler();

    cc.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    cc.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    cc.AHBCLKDivider = RCC_SYSCLK_DIV1;
    cc.APB1CLKDivider = RCC_HCLK_DIV2;
    cc.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&cc, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Pin = BTN_START_PIN;
    HAL_GPIO_Init(BTN_START_PORT, &g);
    g.Pin = BTN_STOP_PIN;
    HAL_GPIO_Init(BTN_STOP_PORT, &g);
    g.Pin = BTN_HIGH_PIN;
    HAL_GPIO_Init(BTN_HIGH_PORT, &g);

    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = LED_RED_PIN | LED_GREEN_PIN | LED_YELLOW_PIN;
    HAL_GPIO_Init(LED_RED_PORT, &g);

    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = UART_BAUD;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();
}

static void MX_TIM2_Init(void)
{
    TIM_MasterConfigTypeDef sm = {0};

    /* APB1=36 MHz → TIM2CLK=72 MHz: PSC=7199, ARR=99 → 100 Hz */
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 7199u;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 99u;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
        Error_Handler();

    sm.MasterOutputTrigger = TIM_TRGO_RESET;
    sm.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &sm);
}

static void MX_TIM3_PWM_Init(void)
{
    TIM_OC_InitTypeDef oc = {0};

    /* TIM3CLK=72 MHz: PSC=71, ARR=999 → 1 kHz PWM */
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 71u;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 999u;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
        Error_Handler();

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, PWM_MOTOR_CHANNEL);
    oc.Pulse = 0;
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, VALVE_CHANNEL);
}

static void apply_pwm_outputs(void)
{
    uint32_t pump = bp_fsm_get_pump_pwm_percent();
    uint32_t valve = bp_fsm_get_valve_pwm_percent();
    if (pump > 100u) pump = 100u;
    if (valve > 100u) valve = 100u;
    __HAL_TIM_SET_COMPARE(&htim3, PWM_MOTOR_CHANNEL, pump * 10u);
    __HAL_TIM_SET_COMPARE(&htim3, VALVE_CHANNEL, valve * 10u);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
        g_tick_100hz = 1u;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uart_proto_rx_callback(huart);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_TIM2_Init();
    MX_TIM3_PWM_Init();

    ads1115_init(&ads, &hi2c1, ADS1115_I2C_ADDR);
    bp_fsm_init();
    uart_proto_init(&huart1);

    HAL_TIM_Base_Start_IT(&htim2);
    HAL_TIM_PWM_Start(&htim3, PWM_MOTOR_CHANNEL);
    HAL_TIM_PWM_Start(&htim3, VALVE_CHANNEL);

    uint32_t seq = 0;
    BpState prev_state = bp_fsm_get_state();

    uart_proto_send_line("A,IDLE\r\n");

    while (1) {
        if (!g_tick_100hz)
            continue;
        g_tick_100hz = 0;

        int start = (HAL_GPIO_ReadPin(BTN_START_PORT, BTN_START_PIN) == GPIO_PIN_RESET) ? 1 : 0;
        int stop = (HAL_GPIO_ReadPin(BTN_STOP_PORT, BTN_STOP_PIN) == GPIO_PIN_RESET) ? 1 : 0;
        int high = (HAL_GPIO_ReadPin(BTN_HIGH_PORT, BTN_HIGH_PIN) == GPIO_PIN_RESET) ? 1 : 0;

        int16_t counts = 0;
        HAL_StatusTypeDef st = ads1115_read_channel0_counts(&ads, &counts);
        if (st != HAL_OK)
            bp_fsm_sensor_i2c_fail();
        else
            bp_fsm_sensor_i2c_ok();

        float p_mmhg = pressure_counts_to_mmhg(counts);

        uint32_t now = HAL_GetTick();
        bp_fsm_on_tick(now, p_mmhg, start, stop, high);

        BpState stt = bp_fsm_get_state();
        if (stt != prev_state) {
            if (stt == BP_STATE_INFLATE_SLOW_LISTEN)
                uart_proto_send_line("A,INFLATE_SLOW\r\n");
            else if (stt == BP_STATE_INFLATE_TO_MARGIN)
                uart_proto_send_line("A,INFLATE_MARGIN\r\n");
            else if (stt == BP_STATE_DEFLATE_MEASURE)
                uart_proto_send_line("A,DEFLATE\r\n");
            else if (stt == BP_STATE_FAST_DEFLATE)
                uart_proto_send_line("A,FAST_DEFLATE\r\n");
            else if (stt == BP_STATE_DONE)
                uart_proto_send_line("E,MEAS_END\r\n");
            else if (stt == BP_STATE_ERROR)
                uart_proto_send_line("E,SENSOR_OR_LEAK\r\n");
            else if (stt == BP_STATE_IDLE && prev_state != BP_STATE_IDLE)
                uart_proto_send_line("A,IDLE\r\n");
        }

        uart_proto_send_sample(seq++, now, p_mmhg);

        apply_pwm_outputs();
        led_hmi_task(bp_fsm_led_hmi_state());

        prev_state = stt;
    }
}
