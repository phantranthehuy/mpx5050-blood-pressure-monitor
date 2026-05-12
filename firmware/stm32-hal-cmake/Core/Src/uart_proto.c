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

/** Hàng đợi xác nhận lệnh — push từ ISR, gửi từ vòng main. */
#define UART_ACK_CAP 8u
static const char *uart_ack_q[UART_ACK_CAP];
static volatile uint32_t uart_ack_wr;
static volatile uint32_t uart_ack_rd;

static void arm_rx(void)
{
    (void)HAL_UART_Receive_IT(g_uart, &rx_byte, 1u);
}

static int upper_char(unsigned char c)
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

static int line_is_start_cmd(const char *line)
{
    while (*line == ' ' || *line == '\t') line++;
    const char *expect = "START";
    for (int i = 0; expect[i] != '\0'; i++) {
        if (line[i] == '\0')
            return 0;
        if (upper_char((unsigned char)line[i]) != (int)expect[i])
            return 0;
    }
    char tail = line[5];
    return (tail == '\0' || tail == '\r' || tail == ' ' || tail == '\t');
}

static int prefix_ci(const char *line, const char *prefix)
{
    for (; *prefix != '\0'; ++line, ++prefix) {
        if (*line == '\0')
            return 0;
        if (upper_char((unsigned char)*line) != (unsigned char)*prefix)
            return 0;
    }
    return 1;
}

static void ack_push_isr(const char *msg)
{
    if (msg == NULL)
        return;
    if (uart_ack_wr - uart_ack_rd >= UART_ACK_CAP)
        return;
    uart_ack_q[uart_ack_wr % UART_ACK_CAP] = msg;
    uart_ack_wr++;
}

void uart_proto_poll_ack_tx(void)
{
    while (uart_ack_rd < uart_ack_wr && g_uart != NULL) {
        const char *m = uart_ack_q[uart_ack_rd % UART_ACK_CAP];
        size_t L = strlen(m);
        (void)HAL_UART_Transmit(g_uart, (uint8_t *)m, (uint16_t)L, 50u);
        uart_ack_rd++;
    }
}

static void handle_cmd_line(const char *line)
{
    while (*line == ' ' || *line == '\t') line++;

    if (line[0] == 'T' || line[0] == 't') {
        float v = 0.f;
        if (sscanf(line + 1, ",%f", &v) == 1) {
            bp_fsm_host_set_target_mmhg(v);
            ack_push_isr("R,OK,T\r\n");
        }
        return;
    }
    if (prefix_ci(line, "SAF,")) {
        float v = 0.f;
        if (sscanf(line + 4, "%f", &v) == 1) {
            bp_fsm_host_set_saf_mmhg(v);
            ack_push_isr("R,OK,SAF\r\n");
        }
        return;
    }
    if (prefix_ci(line, "SAFH,")) {
        float v = 0.f;
        if (sscanf(line + 5, "%f", &v) == 1) {
            bp_fsm_host_set_saf_high_mmhg(v);
            ack_push_isr("R,OK,SAFH\r\n");
        }
        return;
    }
    if (prefix_ci(line, "HIGH,")) {
        int bit = 0;
        if (sscanf(line + 5, "%d", &bit) == 1) {
            bp_fsm_host_set_high_uart(bit ? 1u : 0u);
            ack_push_isr("R,OK,HIGH\r\n");
        }
        return;
    }
    if (line_is_start_cmd(line)) {
        bp_fsm_host_request_uart_start();
        ack_push_isr("R,OK,START\r\n");
        return;
    }
    if (line_is_abort_cmd(line)) {
        bp_fsm_host_abort_measure();
        ack_push_isr("R,OK,ABORT\r\n");
        return;
    }
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
    uart_ack_wr = 0u;
    uart_ack_rd = 0u;
    arm_rx();
}

void uart_proto_send_sample(uint32_t seq, uint32_t t_ms, float p_mmhg)
{
    char line[48];
    /* Tránh %f với newlib nano (printf float thường tắt) — mmHg in bằng số nguyên (×100). */
    float p = p_mmhg;
    if (p < 0.f)
        p = 0.f;
    else if (p > 500.f)
        p = 500.f;
    unsigned long cents = (unsigned long)(p * 100.f + 0.5f);
    int n = snprintf(line, sizeof(line), "S,%lu,%lu,%lu.%02lu\r\n",
                     (unsigned long)seq, (unsigned long)t_ms, cents / 100UL, cents % 100UL);
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
