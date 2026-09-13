#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#include "usb_cdc_acm_adapter.h"
#include "_capture.h"
#include "LA_log.h"

//using enum LA_log::log_level; // TODO figure out

#define TIMEOUT -2

typedef unsigned int uint;

typedef enum sump_flag_bits_t {
    FLAG_DEMUX_MODE = (1 << 0),
    FLAG_NOISE_FILTER = (1 << 1),
    FLAG_DISABLE_CHANGROUP_1 = (1 << 2),
    FLAG_DISABLE_CHANGROUP_2 = (1 << 3),
    FLAG_DISABLE_CHANGROUP_3 = (1 << 4),
    FLAG_DISABLE_CHANGROUP_4 = (1 << 5),
    FLAG_CLOCK_EXTERNAL = (1 << 6),
    FLAG_INVERT_EXT_CLOCK = (1 << 7),
    FLAG_RLE = (1 << 8),
    FLAG_SWAP_CHANNELS = (1 << 9),
    FLAG_EXTERNAL_TEST_MODE = (1 << 10),
    FLAG_INTERNAL_TEST_MODE = (1 << 11),
    FLAG_RESERVED_0 = (1 << 12),
    FLAG_RESERVED_1 = (1 << 13),
    FLAG_RLE_MODE_0 = (1 << 14),
    FLAG_RLE_MODE_1 = (1 << 15)
} sump_flag_bits_t;

class Protocol {
public:
    Protocol(uart_data_interface &uart) : uart(&uart), capture() {};

    void start();

private:
    uart_data_interface* uart;
    Capture capture;
    uint flags;

    void process_cmd(uint8_t cmd);
    void sump_send_samples();
    void sump_reset();
    inline void send_sample(uint sample) {
        if ((flags & FLAG_DISABLE_CHANGROUP_1) == 0) uart->putc(sample);
        if ((flags & FLAG_DISABLE_CHANGROUP_2) == 0) uart->putc(sample >> 8);
    }
    inline void send_sample_rle(uint sample, uint count) {
        if ((flags & FLAG_DISABLE_CHANGROUP_1) || (flags & FLAG_DISABLE_CHANGROUP_2)) {
            uint8_t value = (1 << 7) | (count - 1);
            uart->putc(value);
            send_sample(sample);
            LA_LOG(LA_log::LOG_DEBUG, "Sample: 0x%x Count: %d", sample, count);
        } else {
            uint16_t value = (1 << 15) | (count - 1);
            uart->putc(value);
            uart->putc(value >> 8);
            send_sample(sample);
            LA_LOG(LA_log::LOG_DEBUG, "\nSample: 0x%x Count: %d", sample, count);
        }
    }
    inline bool is_aborting() {
        if (uart->available()) {
            uint8_t cmd = (uint8_t)uart->getc();
            if (cmd == 0x00) {
                LA_LOG(LA_log::LOG_DEBUG, "Capture aborted");
                return true;
            }
        }
        return false;
    }

    inline void put_str(const char* s);
    inline void put_uint32(uint32_t i);
    inline uint32_t get_uint32();
};

#endif // PROTOCOL_H