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

static int line_is_earlyend_cmd(const char *line)
{
    while (*line == ' ' || *line == '\t') line++;
    const char *expect = "EARLYEND";
    for (int i = 0; expect[i] != '\0'; i++) {
        if (line[i] == '\0')
            return 0;
        if (upper_char((unsigned char)line[i]) != (int)expect[i])
            return 0;
    }
    char tail = line[8];
    return (tail == '\0' || tail == '\r' || tail == ' ' || tail == '\t');
}

/** So khớp tiền tố ASCII không phân biệt hoa thường. */
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

static void handle_cmd_line(const char *line)
{
    while (*line == ' ' || *line == '\t') line++;

    if (line[0] == 'T' || line[0] == 't') {
        float v = 0.f;
        if (sscanf(line + 1, ",%f", &v) == 1) {
            bp_fsm_host_set_target_mmhg(v);
            uart_proto_send_line("R,OK,T\r\n");
        }
        return;
    }
    if (prefix_ci(line, "SAF,")) {
        float v = 0.f;
        if (sscanf(line + 4, "%f", &v) == 1) {
            bp_fsm_host_set_saf_mmhg(v);
            uart_proto_send_line("R,OK,SAF\r\n");
        }
        return;
    }
    if (prefix_ci(line, "SAFH,")) {
        float v = 0.f;
        if (sscanf(line + 5, "%f", &v) == 1) {
            bp_fsm_host_set_saf_high_mmhg(v);
            uart_proto_send_line("R,OK,SAFH\r\n");
        }
        return;
    }
    if (prefix_ci(line, "HIGH,")) {
        int bit = 0;
        if (sscanf(line + 5, "%d", &bit) == 1) {
            bp_fsm_host_set_high_uart(bit ? 1u : 0u);
            uart_proto_send_line("R,OK,HIGH\r\n");
        }
        return;
    }
    if (prefix_ci(line, "DR,")) {
        float v = 0.f;
        if (sscanf(line + 3, "%f", &v) == 1) {
            bp_fsm_host_set_deflate_rate_mmhg_s(v);
            uart_proto_send_line("R,OK,DR\r\n");
        }
        return;
    }
    if (line_is_start_cmd(line)) {
        bp_fsm_host_request_uart_start();
        uart_proto_send_line("R,OK,START\r\n");
        return;
    }
    if (line_is_earlyend_cmd(line)) {
        if (bp_fsm_get_state() == BP_STATE_DEFLATE_MEASURE) {
            bp_fsm_host_request_early_measure_done();
            uart_proto_send_line("R,OK,EARLYEND\r\n");
        } else {
            uart_proto_send_line("R,SKIP,EARLYEND\r\n");
        }
        return;
    }
    if (line_is_abort_cmd(line)) {
        bp_fsm_host_abort_measure();
        uart_proto_send_line("R,OK,ABORT\r\n");
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
                                       int rc, int16_t counts,
                                       int fsm, int pump_pct, int valve_pct,
                                       int btn_s, int btn_p, int btn_h, int dp_centi)
{
    char line[128];
    /* Tránh %f: newlib nano trên STM32 thường không link printf float → mmHg bị trống. */
    float p = p_mmhg;
    int sign = (p < 0.f) ? -1 : 1;
    float pa = (sign < 0) ? -p : p;
    if (pa > 500.f)
        pa = 500.f;
    unsigned long cents = (unsigned long)(pa * 100.f + 0.5f);
    /* Full debug line: in cả fsm/pump/valve/buttons mỗi tick để chẩn đoán
     * vì sao bấm START mà motor không bơm (state có chuyển không, PWM có
     * lên không, nút STOP có bị stuck low không). */
    int n = snprintf(line, sizeof(line),
                     "S,%lu,%lu,%s%lu.%02lu,%d,%d,%d,%d,%d,%d%d%d,%d\r\n",
                     (unsigned long)seq, (unsigned long)t_ms,
                     (sign < 0) ? "-" : "",
                     cents / 100UL, cents % 100UL,
                     rc, (int)counts,
                     fsm, pump_pct, valve_pct,
                     btn_s ? 1 : 0, btn_p ? 1 : 0, btn_h ? 1 : 0,
                     dp_centi);
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
