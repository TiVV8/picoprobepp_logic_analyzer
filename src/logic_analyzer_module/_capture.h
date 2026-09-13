#ifndef _CAPTURE_H
#define _CAPTURE_H

#include <array>

#include "gpio_rp2040.h"
#include "pio_rp2040.h"
#define pio_rp2xxx pio_rp2040
#include "dma_rp2040.h"
#define dma_rp2xxx dma_rp2040
#include "gpio_rp2040.h"
#define gpio_rp2xxx gpio_rp2040

#include "config.h"

#include "LA_log.h"

#define STAGE_COUNT 4
#define TRIGGER_COUNT 4

#define TRIGGER_START (1 << (3 + 24))
#define TRIGGER_SERIAL (1 << (2 + 24))
#define TRIGGER_CHANNEL_MASK (31 << (4 + 16))
#define TRIGGER_CHANNEL(NUMBER) (NUMBER << (4 + 16))
#define TRIGGER_LEVEL_MASK (3 << (0 + 24))
#define TRIGGER_LEVEL(NUMBER) (NUMBER << (0 + 24))

#define PRE_TRIGGER_RING_BITS 10
#define PRE_TRIGGER_BUFFER_SIZE (1 << PRE_TRIGGER_RING_BITS)
#define PRE_TRIGGER_RING_TRANSFER_COUNT ((0xffffffffu / PRE_TRIGGER_BUFFER_SIZE) * PRE_TRIGGER_BUFFER_SIZE)
#define POST_TRIGGER_BUFFER_SIZE 100000

#define PIN_BASE (gpio_pin_t)18

typedef unsigned int uint;

typedef enum trigger_match_t {
    TRIGGER_TYPE_LEVEL_LOW,
    TRIGGER_TYPE_LEVEL_HIGH,
    TRIGGER_TYPE_EDGE_LOW,
    TRIGGER_TYPE_EDGE_HIGH
} trigger_match_t;

typedef struct trigger_t {
    bool is_enabled = false;
    uint pin;
    trigger_match_t match;
} trigger_t;

typedef struct trigger_stage_config_t {
    uint mask = 0;
    uint values = 0;
    uint configuration = 0x00009008;
} trigger_stage_config_t;

typedef struct capture_config_t {
    uint total_samples = MAX_TOTAL_SAMPLES;
    uint rate = MAX_SAMPLE_RATE;
    uint pre_trigger_samples = 1000;
    uint channels = MAX_CHANNELS;
    gpio_pin_t base = PIN_BASE;
    trigger_t triggers[TRIGGER_COUNT];
    trigger_stage_config_t stages[STAGE_COUNT];
    bool trigger_edge;
} capture_config_t;

class Capture {
public:
    Capture();

    void capture_start();
    void capture_abort();
    uint get_sample(uint index);

    bool capture_is_busy() {return is_capturing;}
    bool is_capture_complete() {
        if (capture_completed) {
            capture_completed = false;
            return true;
        } else {
            return false;
        }
    }
    capture_config_t* get_config(){return &config;};
    uint get_pre_trigger_count(){return pre_trigger_count;};
    uint get_sample_count(){return pre_trigger_count + config.total_samples - config.pre_trigger_samples;};
    int get_triggered_channel(){return triggered_channel;};
private:
    const uint triggered_channel_index[4] = {0, 1, 2, 3};
    uint pio0_ctrl;
    uint pio1_ctrl = 0;

    capture_config_t config;
    uint pre_trigger_count;
    int triggered_channel, pre_trigger_first;
    bool is_capturing, is_aborting, capture_completed;
    std::array<uint16_t, PRE_TRIGGER_BUFFER_SIZE> pre_trigger_buffer{};
    std::array<uint16_t, POST_TRIGGER_BUFFER_SIZE> post_trigger_buffer{};

    // State Machine Channels (GPIO Pins)
    gpio_rp2xxx* la_channels[MAX_CHANNELS];

    // PIO State Machines
    std::unique_ptr<SM> mux_sm;
    std::unique_ptr<SM> pre_trigger_sm;
    std::unique_ptr<SM> post_trigger_sm = nullptr;
    std::unique_ptr<SM> trigger_sms[TRIGGER_COUNT];

    // DMA channels
    std::unique_ptr<dma_rp2xxx> dma_pio0_ctrl;
    std::unique_ptr<dma_rp2xxx> dma_pio1_ctrl;
    std::unique_ptr<dma_rp2xxx> dma_pre_trigger;
    std::unique_ptr<dma_rp2xxx> dma_post_trigger;
    std::unique_ptr<dma_rp2xxx> dma_trigger[4];


    void apply_trigger_config();  // prepare_adquisition() in codebase
    void capture_complete_handler();
    void set_trigger(uint i);
    void capture_stop();
    void clear_fifo(std::unique_ptr<SM>& sm);
};

#endif // CAPTURE_H