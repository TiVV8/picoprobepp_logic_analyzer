#include "_capture.h"
#include "capture.pio.h"
#include "LA_log.h"
#include "task.h"

//using enum LA_log::log_level; // TODO figure out

Capture::Capture() {
    is_capturing = is_aborting = capture_completed = false;
    triggered_channel= -1;
    
    // Init Logic Analyzer channels
    for (int ch=0; ch < 12; ch++) {
        gpio_rp2xxx gpio(ch + 18);
        gpio.gpioMode(GPIO::INPUT | GPIO::PULLUP);
        la_channels[ch] = &gpio;
    }
    LA_LOG(LA_log::LOG_DEBUG, "Channel init complete");

    // Read Trigger type
    gpio_rp2xxx type_pin(17);
    type_pin.gpioMode(GPIO::INPUT | GPIO::PULLUP);
    config.trigger_edge = type_pin.gpioRead();
}

void Capture::capture_start() {
    apply_trigger_config();

    LA_LOG(LA_log::LOG_DEBUG, "Sys Clk: %d Clk div: %d", CLK_SYS, CLK_SYS / config.rate);

    // Load mux program on to SM
    mux_sm = pio_rp2xxx::pio1.loadProgram(mux_program);
    configure_pio_mux(mux_sm, [this](){
        // Get Triggered Channel
        triggered_channel = mux_sm->readRxFifo();
        // Clear Interrupt
        mux_sm->pio.IRQ = 1;
    });
    LA_LOG(LA_log::LOG_DEBUG, "Mux SM initialized");

    // Allocate DMA channel for disable mux and triggers
    dma_pio1_ctrl = dma_rp2xxx::allocateChannel();
    dma_pio1_ctrl->CTRL.EN = true;
    // Set size of bus transfer
    dma_pio1_ctrl->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_WORD;
    // Set read and write increment
    dma_pio1_ctrl->CTRL.INCR_READ = false;
    dma_pio1_ctrl->CTRL.INCR_WRITE = false;
    // Set number of transfers
    dma_pio1_ctrl->TRANS_COUNT = 1;
    // Set read and write addresses
    dma_pio1_ctrl->READ_ADDR = (uint32_t)&pio1_ctrl;
    dma_pio1_ctrl->WRITE_ADDR = (uint32_t)&_PIO1_::PIO1.CTRL;
    LA_LOG(LA_log::LOG_DEBUG, "PIO1 control DMA initialized");

    // Allocate DMA channel for disable pre trigger and enable post trigger
    dma_pio0_ctrl = dma_rp2xxx::allocateChannel();
    dma_pio0_ctrl->CTRL.EN = true;
    // Set size of bus transfer
    dma_pio0_ctrl->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_WORD;
    // Set read and write increment
    dma_pio0_ctrl->CTRL.INCR_READ = false;
    dma_pio0_ctrl->CTRL.INCR_WRITE = false;
    // Set DREQ
    // Calculate correct DREQ like the following: PIO + TX + Index
        // PIO: PIO0 = 0; PIO1 = 8
        // TX: Send to SM (TX) = 0; Read from SM (RX) = 4
        // Index: Index of the SM e.g. 0-3
    // Here: PIO0 + RX + Index = 4 + Index
    dma_pio0_ctrl->CTRL.TREQ_SEL = (4 + mux_sm->sm_index);
    // Chain to different DMA channel
    dma_pio0_ctrl->CTRL.CHAIN_TO = dma_pio1_ctrl->getIndex();
    // Set number of transfers
    dma_pio0_ctrl->TRANS_COUNT = 1;
    // Set read and write addresses
    dma_pio0_ctrl->READ_ADDR = (uint32_t)&pio0_ctrl;
    dma_pio0_ctrl->WRITE_ADDR = (uint32_t)&_PIO0_::PIO0.CTRL;
    // Start channel
    dma_pio0_ctrl->trigger();
    LA_LOG(LA_log::LOG_DEBUG, "PIO0 control DMA initialized");

    // Init pre trigger samples
    // Load capture program for pre trigger samples on to SM
    pre_trigger_sm = pio_rp2xxx::pio1.loadProgram(capture_program);
    configure_capture(pre_trigger_sm, config);
    LA_LOG(LA_log::LOG_DEBUG, "Pre trigger SM initialized");

    // Allocate DMA channel for pre trigger samples
    dma_pre_trigger = dma_rp2xxx::allocateChannel();
    dma_pre_trigger->CTRL.EN = true;
    // Set size of bus transfer
    dma_pre_trigger->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_HALFWORD;
    // Enable ring buffer
    dma_pre_trigger->CTRL.RING_SEL = true;
    dma_pre_trigger->CTRL.RING_SIZE = PRE_TRIGGER_RING_BITS + 1;
    // Set read and write increment
    dma_pre_trigger->CTRL.INCR_READ = false;
    dma_pre_trigger->CTRL.INCR_WRITE = true;
    // Set DREQ
    dma_pre_trigger->CTRL.TREQ_SEL = (4 + pre_trigger_sm->sm_index);
    // Restart channel at the end of each transfer sequence
    dma_pre_trigger->attachIrq0([this](){
        // Clear Interrupt
        DMA_SET.INTS0 = DMA.INTS0;
        // Restart Pre-Trigger DMA
        dma_pre_trigger->trigger();
    });
    // Set number of transfers
    dma_pre_trigger->TRANS_COUNT = PRE_TRIGGER_RING_TRANSFER_COUNT;
    // Set read and write addresses
    dma_pre_trigger->READ_ADDR = (uint32_t)&pre_trigger_sm->pio.RXF[pre_trigger_sm->sm_index];
    dma_pre_trigger->WRITE_ADDR = (uint32_t)&pre_trigger_buffer[0];
    // Trigger channel
    dma_pre_trigger->trigger();

    LA_LOG(LA_log::LOG_DEBUG, "Pre trigger DMA initialized");


    // Init post trigger samples
    // Load capture program for post trigger samples on to SM
    post_trigger_sm = pio_rp2xxx::pio1.loadProgram(capture_program);
    configure_capture(post_trigger_sm, config);
    // Set pio0_ctrl
    pio0_ctrl = (1 << post_trigger_sm->sm_index);
    LA_LOG(LA_log::LOG_DEBUG, "Post trigger SM initialized");

    // Allocate DMA channel for post trigger samples
    dma_post_trigger = dma_rp2xxx::allocateChannel();
    dma_post_trigger->CTRL.EN = true;
    // Set size of bus transfer
    dma_post_trigger->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_HALFWORD;
    // Set read and write increment
    dma_post_trigger->CTRL.INCR_READ = false;
    dma_post_trigger->CTRL.INCR_WRITE = true;
    // Set DREQ
    dma_post_trigger->CTRL.TREQ_SEL = (4 + post_trigger_sm->sm_index);
    // Set interrupt for capture complete
    dma_post_trigger->attachIrq0([this](){capture_complete_handler();});
    // Set number of transfers
    //dma_post_trigger->TRANS_COUNT = config.total_samples - config.pre_trigger_samples;
    dma_post_trigger->TRANS_COUNT = 500;
    // Set read and write addresses
    dma_post_trigger->READ_ADDR = (uint32_t)&post_trigger_sm->pio.RXF[post_trigger_sm->sm_index];
    dma_post_trigger->WRITE_ADDR = (uint32_t)&post_trigger_buffer[0];
    // Trigger channel
    dma_post_trigger->trigger();
    LA_LOG(LA_log::LOG_DEBUG, "Post trigger DMA initialized");


    // Init triggers
    uint i = 0;
    while (config.triggers[i].is_enabled) {
        set_trigger(i);
        i++;
    }

    // Start state machines
    if (!i) {
        // Enable post trigger SM
        post_trigger_sm->enable();
        LA_LOG(LA_log::LOG_INFO, "No trigger enabled. Started post trigger SM");
    } else {
        // Enable pre trigger SM
        pre_trigger_sm->enable();
        // Enable mux SM
        mux_sm->enable();
        // Enable trigger SMs
        for (auto&& sm : trigger_sms) {
            if (sm != nullptr) {
                sm->enable();
            } else {
                break;
            }
        }
        LA_LOG(LA_log::LOG_INFO, "%d trigger enabled. Started pre trigger, mux and trigger SMs", i);
    }

    is_capturing = true;

    LA_LOG(LA_log::LOG_INFO, "Capture start. Samples: %d Rate: %d Pre trigger samples: %d", config.total_samples, config.rate, config.pre_trigger_samples);
}

void Capture::apply_trigger_config() {
    // Disable all trigger
    for (int i=0; i < TRIGGER_COUNT; i++) {
        config.triggers[i].is_enabled = false;
    }

    uint trigger = 0;
    for (int stage=0; stage < STAGE_COUNT; stage++) {
        trigger_stage_config_t stage_conf = config.stages[stage];

        LA_LOG(LA_log::LOG_DEBUG, "Stage: %d Mask: 0x%x Values: 0x%x Configuration: 0x%x", stage, stage_conf.mask, stage_conf.values, stage_conf.configuration);

        if (stage_conf.mask &&
            ((stage_conf.configuration & TRIGGER_START) &&
            ((stage_conf.configuration & TRIGGER_LEVEL_MASK) == 0))
        ) {
            if (!(stage_conf.configuration & TRIGGER_SERIAL)) {  // Level triggers (parallel trigger)
                for (uint channel = 0; channel < config.channels; channel++) {
                    if (((stage_conf.mask >> channel) & 1) != 0) {
                        config.triggers[trigger].is_enabled = true;
                        config.triggers[trigger].pin = channel;

                        if (!config.trigger_edge) {
                            if (((stage_conf.values >> channel) & 1) == 1) {
                                config.triggers[trigger].match = TRIGGER_TYPE_LEVEL_HIGH;
                            } else {
                                config.triggers[trigger].match = TRIGGER_TYPE_LEVEL_LOW;
                            }
                        } else {
                            if (((stage_conf.values >> channel) & 1) == 1) {
                                config.triggers[trigger].match = TRIGGER_TYPE_EDGE_HIGH;
                            } else {
                                config.triggers[trigger].match = TRIGGER_TYPE_EDGE_LOW;
                            }
                        }

                        trigger++;
                    }
                }
            } else if ((stage_conf.configuration & TRIGGER_SERIAL) && (stage_conf.mask == 0b11)) {  // Edge triggers (serial, mask 0b11)
                config.triggers[trigger].is_enabled = true;
                config.triggers[trigger].pin = (stage_conf.configuration & TRIGGER_CHANNEL_MASK) >> 20;

                if (((stage_conf.values & 1) == 0) && (((stage_conf.values >> 1) & 1) == 1)) {
                    config.triggers[trigger].match = TRIGGER_TYPE_EDGE_HIGH;
                    trigger++;
                } else if (((stage_conf.values & 1) == 1) &&
                           (((stage_conf.values >> 1) & 1) == 0)) {
                    config.triggers[trigger].match = TRIGGER_TYPE_EDGE_LOW;
                    trigger++;
                } else {
                    config.triggers[trigger].is_enabled = false;
                }
            } else if ((stage_conf.configuration & TRIGGER_SERIAL) && (stage_conf.mask == 0b1)) {
                config.triggers[trigger].is_enabled = true;
                config.triggers[trigger].pin =
                    (stage_conf.configuration & TRIGGER_CHANNEL_MASK) >> 20;

                if ((stage_conf.values & 1) == 1) {
                    config.triggers[trigger].match = TRIGGER_TYPE_LEVEL_HIGH;
                    trigger++;
                } else {
                    config.triggers[trigger].match = TRIGGER_TYPE_LEVEL_LOW;
                    trigger++;
                }
            }

            if (trigger > TRIGGER_COUNT - 1) {
                LA_LOG(LA_log::LOG_INFO, "Trigger ignored. Reached maximum number of triggers (%d)", TRIGGER_COUNT);
                return;
            }
        }
    }

    if (trigger < TRIGGER_COUNT) {
        config.triggers[trigger].is_enabled = false;
    }
}

void Capture::set_trigger(uint i) {
    if (i < TRIGGER_COUNT) {
        // Load trigger program on to SM
        switch (config.triggers[i].match) {
        case TRIGGER_TYPE_LEVEL_HIGH:
            trigger_sms[i] = pio_rp2xxx::pio0.loadProgram(trigger_level_high_program);
            break;
        case TRIGGER_TYPE_LEVEL_LOW:
            trigger_sms[i] = pio_rp2xxx::pio0.loadProgram(trigger_level_low_program);
            break;
        case TRIGGER_TYPE_EDGE_HIGH:
            trigger_sms[i] = pio_rp2xxx::pio0.loadProgram(trigger_edge_high_program);
            break;
        case TRIGGER_TYPE_EDGE_LOW:
            trigger_sms[i] = pio_rp2xxx::pio0.loadProgram(trigger_edge_low_program);
            break;
        }
        configure_trigger(trigger_sms[i], config.rate, config.base);

        // Allocate DMA channel for trigger
        dma_trigger[i] = dma_rp2xxx::allocateChannel();
        dma_trigger[i]->CTRL.EN = true;
        // Set size of bus transfer
        dma_trigger[i]->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_WORD;
        // Set read and write increment
        dma_trigger[i]->CTRL.INCR_READ = false;
        dma_trigger[i]->CTRL.INCR_WRITE = false;
        // Set DREQ
        dma_trigger[i]->CTRL.TREQ_SEL = (12 + trigger_sms[i]->sm_index);
        // Set number of transfers
        dma_trigger[i]->TRANS_COUNT = 1;
        // Set read and write addresses
        dma_trigger[i]->READ_ADDR = (uint32_t)&triggered_channel_index[i];
        dma_trigger[i]->WRITE_ADDR = (uint32_t)&mux_sm->pio.TXF[mux_sm->sm_index];
        // Trigger channel
        dma_trigger[i]->trigger();

        LA_LOG(
            LA_log::LOG_DEBUG,
            "Set trigger %d Pin: %d Match: %s %s",
            i, config.triggers[i].pin, config.triggers[i].match, config.trigger_edge ? "(override)" : ""
        );
    }
}

void Capture::capture_complete_handler() {
    if (!is_aborting) {
        pre_trigger_first = 0;
        pre_trigger_count = 0;
        /* DEBUG
        if (config.pre_trigger_samples) {
            uint transfer_count = dma_pre_trigger->TRANS_COUNT;
            pre_trigger_first = (int)(transfer_count % PRE_TRIGGER_BUFFER_SIZE) - (int)config.pre_trigger_samples;
            pre_trigger_count = config.pre_trigger_samples;
            // TODO Check if this works even though first may be negative
            if ((pre_trigger_first < 0) && (transfer_count < PRE_TRIGGER_BUFFER_SIZE)) {
                pre_trigger_first = 0;
                pre_trigger_count = transfer_count;
            }
        }
        */
        capture_stop();
        is_capturing = false;
        capture_completed = true;
    } else {
        is_aborting = false;
    }
}

uint Capture::get_sample(uint index) {
    /* DEBUG
    if (index >= get_sample_count()) return 0;

    if (index < pre_trigger_count) {
        int pos = pre_trigger_first + index;

        if (pos < 0) {
            pos += PRE_TRIGGER_BUFFER_SIZE;
        } else if (pos >= PRE_TRIGGER_BUFFER_SIZE) {
            pos -= PRE_TRIGGER_BUFFER_SIZE;
        }
        return pre_trigger_buffer[pos];
    }

    return post_trigger_buffer[index - pre_trigger_count];
    */
   return post_trigger_buffer[index];
}

void Capture::capture_abort() {
    is_capturing = false;
    is_aborting = true;
    capture_stop();
}

void Capture::capture_stop() {
    // Disable all SMs
    mux_sm->disable();
    pre_trigger_sm->disable();
    post_trigger_sm->disable();
    for (auto&& sm : trigger_sms) {
        if (sm != nullptr) {
            sm->disable();
        }
    }

    // Free DMA channels
    dma_pio0_ctrl.reset();
    dma_pio1_ctrl.reset();
    dma_pre_trigger.reset();
    dma_post_trigger.reset();
    for (auto&& dma : dma_trigger) {
        if (dma != nullptr) {
            dma.reset();
        }
    }

    // Clear SM FIFOs
    clear_fifo(mux_sm);
    clear_fifo(pre_trigger_sm);
    clear_fifo(post_trigger_sm);
    for (auto&& sm : trigger_sms) {
        if (sm != nullptr) {
            clear_fifo(sm);
        }
    }
    // TODO Clear SM instructions
}

void Capture::clear_fifo(std::unique_ptr<SM>& sm) {
    sm->regs.SM_SHIFTCTRL.FJOIN_RX = !(sm->regs.SM_SHIFTCTRL.FJOIN_RX);
    sm->regs.SM_SHIFTCTRL.FJOIN_RX = !(sm->regs.SM_SHIFTCTRL.FJOIN_RX);
}