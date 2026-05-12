#include "uart_proto.h"
#include "board_config.h"
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include "bp_fsm.h"
}

#define RX_BUF_LEN 128
static char rx_buf[RX_BUF_LEN];
static uint8_t rx_idx;

static int upper_char(char c)
{
    if (c >= 'a' && c <= 'z')
        return (int)c - 32;
    return (int)c;
}

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

extern "C" void uart_proto_init(void)
{
    Serial.begin(UART_BAUD);
    rx_idx = 0;
}

extern "C" void uart_proto_poll_rx(void)
{
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) break;
        feed_rx_byte((uint8_t)c);
    }
}

extern "C" void uart_proto_send_sample(uint32_t seq, uint32_t t_ms, float p_mmhg,
                                       int rc, int16_t counts)
{
    char line[80];
    /* Tránh %f: newlib nano trên STM32 thường không link printf float → mmHg bị trống. */
    float p = p_mmhg;
    int sign = (p < 0.f) ? -1 : 1;
    float pa = (sign < 0) ? -p : p;
    if (pa > 500.f)
        pa = 500.f;
    unsigned long cents = (unsigned long)(pa * 100.f + 0.5f);
    /* Bổ sung rc và counts ở cuối: S,<seq>,<t_ms>,<p_mmhg>,<rc>,<counts>
     * → mỗi dòng đều thấy ADC thô, không cần ngó dòng D,... rời (đã bỏ).
     * In luôn dấu '-' nếu p âm để biết đang bị clip vì lý do gì. */
    int n = snprintf(line, sizeof(line), "S,%lu,%lu,%s%lu.%02lu,%d,%d\r\n",
                     (unsigned long)seq, (unsigned long)t_ms,
                     (sign < 0) ? "-" : "",
                     cents / 100UL, cents % 100UL,
                     rc, (int)counts);
    if (n > 0)
        Serial.write((const uint8_t *)line, (size_t)n);
}

extern "C" void uart_proto_send_line(const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0)
        Serial.write((const uint8_t *)buf, (size_t)n);
}
