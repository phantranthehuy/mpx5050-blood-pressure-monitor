#include "uart_proto.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bp_fsm.h"

static UART_HandleTypeDef *g_uart;
static uint8_t rx_byte;

#define RX_BUF_LEN 128
static char rx_buf[RX_BUF_LEN];
static uint8_t rx_idx;

static void arm_rx(void)
{
    (void)HAL_UART_Receive_IT(g_uart, &rx_byte, 1u);
}

static int upper_char(char c)
{
    if (c >= 'a' && c <= 'z')
        return (int)c - 32;
    return (int)c;
}

/** Khớp "ABORT" (không phân biệt hoa thường) hoặc chỉ "A". */
static int line_is_abort_cmd(const char *line)
{
    while (*line == ' ' || *line == '\t') line++;
    if (upper_char(line[0]) == 'A' && (line[1] == '\0' || line[1] == '\r'))
        return 1;
    const char *expect = "ABORT";
    for (int i = 0; expect[i] != '\0'; i++) {
        if (upper_char(line[i]) != (int)expect[i])
            return 0;
    }
    char tail = line[5];
    return (tail == '\0' || tail == '\r' || tail == ' ' || tail == '\t');
}

static void handle_cmd_line(const char *line)
{
    while (*line == ' ' || *line == '\t') line++;

    if (line[0] == 'T' || line[0] == 't') {
        float v = 0.f;
        if (sscanf(line + 1, ",%f", &v) == 1)
            bp_fsm_host_set_target_mmhg(v);
        return;
    }
    if (line_is_abort_cmd(line))
        bp_fsm_host_abort_measure();
}

static void feed_rx_byte(uint8_t b)
{
    if (b == '\r') return;
    if (b == '\n') {
        if (rx_idx >= RX_BUF_LEN) rx_idx = RX_BUF_LEN - 1u;
        rx_buf[rx_idx] = '\0';
        rx_idx = 0;
        handle_cmd_line(rx_buf);
        return;
    }
    if (rx_idx < RX_BUF_LEN - 1u) rx_buf[rx_idx++] = (char)b;
    else rx_idx = 0;
}

void uart_proto_init(UART_HandleTypeDef *huart)
{
    g_uart = huart;
    rx_idx = 0;
    arm_rx();
}

void uart_proto_send_sample(uint32_t seq, uint32_t t_ms, float p_mmhg)
{
    char line[48];
    int n = snprintf(line, sizeof(line), "S,%lu,%lu,%.2f\r\n",
                     (unsigned long)seq, (unsigned long)t_ms, p_mmhg);
    if (n > 0 && g_uart != NULL)
        (void)HAL_UART_Transmit(g_uart, (uint8_t *)line, (uint16_t)n, 20u);
}

void uart_proto_send_line(const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0 && g_uart != NULL)
        (void)HAL_UART_Transmit(g_uart, (uint8_t *)buf, (uint16_t)n, 50u);
}

void uart_proto_rx_callback(UART_HandleTypeDef *huart)
{
    if (huart != g_uart) return;
    feed_rx_byte(rx_byte);
    arm_rx();
}
