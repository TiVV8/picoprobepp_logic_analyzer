#include "task.h"

#include "protocol.h"
#include "_capture.h"
#include "LA_log.h"

#include "config.h"

//using enum LA_log::log_level; // TODO figure out

void Protocol::start() {
    int i = 0;
    while (true) {
        if (uart->available()) {
            uint8_t cmd = (uint8_t)uart->getc();
            LA_LOG(LA_log::LOG_DEBUG, "Received Command: 0x%x", cmd);
            process_cmd(cmd);
        }
        if (capture.is_capture_complete()) {
            LA_LOG(
                LA_log::LOG_INFO,
                "Capture complete. Samples count: %d Pre trigger count: %d\nSending Samples",
                capture.get_sample_count(), capture.get_pre_trigger_count()
            );
            /* DEBUG
            if (capture.get_triggered_channel() != -1) {
                LA_LOG(LA_log::LOG_DEBUG, "\nTriggered channel: %d", capture.get_triggered_channel());
            }
            if (capture.get_pre_trigger_count() < capture.get_config()->pre_trigger_samples) {
                LA_LOG(
                    LA_log::LOG_WARNING,
                    "Warning. Not enough pre trigger samples. Missing samples (%d) will be sent as 0x0000 samples",
                    capture.get_config()->pre_trigger_samples - capture.get_pre_trigger_count()
                );
            }
            */

            sump_send_samples();
        }

        if (i == 0) {
            // capture.test();
        }
        i = (i + 1) % 10000000;
    }
}


void Protocol::process_cmd(uint8_t cmd) {
    switch (cmd) {
    case 0x00:  // reset
    {
        LA_LOG(LA_log::LOG_INFO, "Reset (0x%x)", cmd);
        
        if (capture.capture_is_busy()) {
            capture.capture_abort();
        }
        sump_reset();
        break;
    }
    case 0x01:  // run
        LA_LOG(LA_log::LOG_INFO, "Run (0x%x)...", cmd);

        capture.capture_start();
        break;
    case 0x02:  // send id
        put_str("1ALS");

        LA_LOG(LA_log::LOG_INFO, "Send ID (0x%x)", cmd);
        break;
    case 0x04:  // send metadata
        // device name
        uart->putc(0x01);
        put_str(DEVICE_NAME);
        uart->putc(0x00);
        // firmware version
        uart->putc(0x02);
        put_str(DEVICE_VERSION);
        uart->putc(0x00);
        // sample memory
        uart->putc(0x21);
        put_uint32(MAX_TOTAL_SAMPLES);
        // sample rate
        uart->putc(0x23);
        put_uint32(MAX_SAMPLE_RATE);
        // number of channels
        uart->putc(0x40);
        uart->putc(MAX_CHANNELS);
        // protocol version
        uart->putc(0x41);
        uart->putc(PROTOCOL_VERSION);
        // eof
        uart->putc(0x00);
        
        LA_LOG(
            LA_log::LOG_INFO,
            "Send metadata (0x%x):\n-Name: %s\n-Version: %s\n-Max samples: %d\n-Max rate: %d\n-Probes: %d\n-Protocol: %d",
            cmd, DEVICE_NAME, DEVICE_VERSION, MAX_TOTAL_SAMPLES, MAX_SAMPLE_RATE, MAX_CHANNELS, PROTOCOL_VERSION
        );
        break;
    // stage 0
    case 0xC0:  // trigger mask stage 0
        capture.get_config()->stages[0].mask = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 0 mask (0x%x): %d", cmd, capture.get_config()->stages[0].mask);
        break;
    case 0xC1:  // trigger values stage 0
        capture.get_config()->stages[0].values = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 0 values (0x%x): 0x%x", cmd, capture.get_config()->stages[0].values);
        break;
    case 0xC2:  // trigger configuration stage 0
        capture.get_config()->stages[0].configuration = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 0 configuration (0x%x): 0x%x", cmd, capture.get_config()->stages[0].configuration);
        break;
    // stage 1
    case 0xC4:  // trigger mask stage 1
        capture.get_config()->stages[1].mask = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 1 mask (0x%x): %d", cmd, capture.get_config()->stages[1].mask);
        break;
    case 0xC5:  // trigger values stage 1
        capture.get_config()->stages[1].values = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 1 values (0x%x): 0x%x", cmd, capture.get_config()->stages[1].values);
        break;
    case 0xC6:  // trigger configuration stage 1
        capture.get_config()->stages[1].configuration = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 1 configuration (0x%x): 0x%x", cmd, capture.get_config()->stages[1].configuration);
        break;
    // stage 2
    case 0xC8:  // trigger mask stage 2
        capture.get_config()->stages[2].mask = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 2 mask (0x%x): %d", cmd, capture.get_config()->stages[2].mask);
        break;
    case 0xC9:  // trigger values stage 2
        capture.get_config()->stages[2].values = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 2 values (0x%x): 0x%x", cmd, capture.get_config()->stages[2].values);
        break;
    case 0xCA:  // trigger configuration stage 2
        capture.get_config()->stages[2].configuration = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 2 configuration (0x%x): 0x%x", cmd, capture.get_config()->stages[2].configuration);
        break;
    // stage 3
    case 0xCC:  // trigger mask stage 3
        capture.get_config()->stages[3].mask = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 3 mask (0x%x): %d", cmd, capture.get_config()->stages[3].mask);
        break;
    case 0xCD:  // trigger values stage 3
        capture.get_config()->stages[3].values = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 3 values (0x%x): 0x%x", cmd, capture.get_config()->stages[3].values);
        break;
    case 0xCE:  // trigger configuration stage 3
        capture.get_config()->stages[3].configuration = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read trigger stage 3 configuration (0x%x): 0x%x", cmd, capture.get_config()->stages[3].configuration);
        break;
    case 0x80:  // divisor
    {
        uint div = get_uint32();
        capture.get_config()->rate = CLK_SYS / (div + 1);
        LA_LOG(LA_log::LOG_INFO, "Read divisor (0x%x): %d", cmd, (div + 1));
        break;
    }
    case 0x81:  // sample size & pre trigger size
    {
        uint32_t value = get_uint32();
        capture.get_config()->total_samples = ((uint16_t)value * 4 + 4);
        capture.get_config()->pre_trigger_samples = capture.get_config()->total_samples - (((value >> 16) * 4) + 4);
        LA_LOG(LA_log::LOG_INFO, "Read samples (0x%x): %d", cmd, capture.get_config()->total_samples);
        LA_LOG(LA_log::LOG_INFO, "Read pre trigger samples (0x%x): %d", cmd, capture.get_config()->pre_trigger_samples);
        break;
    }
    case 0x82:  // flags. samplerate <= clock rate: demux off. samplerate > clock rate: demux on
        flags = get_uint32();
        LA_LOG(
            LA_log::LOG_INFO, 
            "Read flags (0x%x): 0x%x\n-Demux: %s -> Rate: %d\n-RLE: %s\n-Channel group 1: %s\n-Channel group 2: %s\n-Channel group 3: %s\n-Channel group 4: %s",
            cmd,
            flags, flags & FLAG_DEMUX_MODE ? "enabled" : "disabled",
            capture.get_config()->rate,
            flags & FLAG_RLE ? "enabled" : "disabled",
            flags & FLAG_DISABLE_CHANGROUP_1 ? "disabled" : "enabled",
            flags & FLAG_DISABLE_CHANGROUP_2 ? "disabled" : "enabled",
            flags & FLAG_DISABLE_CHANGROUP_3 ? "disabled" : "enabled",
            flags & FLAG_DISABLE_CHANGROUP_4 ? "disabled" : "enabled"
        );
        break;
    case 0x83:  // sample size
        capture.get_config()->total_samples = get_uint32();
        LA_LOG(LA_log::LOG_INFO, "Read samples (0x%x): %d", cmd, capture.get_config()->total_samples);
        break;
    case 0x84:  // pre trigger size
    {
        uint32_t value = get_uint32();
        capture.get_config()->pre_trigger_samples = capture.get_config()->total_samples - ((uint16_t)value * 4 + 4);
        LA_LOG(LA_log::LOG_INFO, "Read pre trigger samples (0x%x): %d", cmd, capture.get_config()->pre_trigger_samples);
        break;
    }
    default:
        LA_LOG(LA_log::LOG_INFO, "\nUnknown command: 0x%x", cmd);
        break;
    }
}

void Protocol::sump_send_samples() {
    LA_LOG(LA_log::LOG_INFO, "Send samples. RLE %s", flags & FLAG_RLE ? "enabled" : "disabled");

    int min_index = capture.get_sample_count() - capture.get_config()->total_samples;

    if (flags & FLAG_RLE) {
        uint channelgroup_mask = 0;
        if ((flags & FLAG_DISABLE_CHANGROUP_1) == 0) channelgroup_mask = 0xff;
        if ((flags & FLAG_DISABLE_CHANGROUP_2) == 0) channelgroup_mask |= 0xff << 8;

        int samples_count = capture.get_sample_count();
        int index = samples_count - 1;
        uint sample = capture.get_sample(index) & channelgroup_mask;
        uint sample_prev = sample;
        uint rle_max_count = (flags & FLAG_DISABLE_CHANGROUP_1) || (flags & FLAG_DISABLE_CHANGROUP_2)
                                 ? (0xff >> 1) + 1
                                 : (0xffff >> 1) + 1;

        while (index > min_index) {
            if (is_aborting()) return;

            uint rle_count = 0;
            do {
                index--;
                rle_count++;
                sample_prev = sample;
                sample = capture.get_sample(index) & channelgroup_mask;
            } while ((sample == sample_prev) && index >= min_index && rle_count < rle_max_count);

            send_sample_rle(sample_prev, rle_count);
        }
        
    } else {
        //for (int i=capture.get_sample_count()-1; i >= min_index; i--) { DEBUG
        for (int i=500-1; i >= 0; i--) {
            if (is_aborting()) return;
            uint sample = capture.get_sample(i);
            send_sample(sample);
            //LA_LOG(LA_log::LOG_DEBUG, "Sample %d: 0x%x", i - min_index, sample); DEBUG
            LA_LOG(LA_log::LOG_DEBUG, "Sample %d: 0x%x", i, sample);
        }
    }
    LA_LOG(LA_log::LOG_DEBUG, "Transfer completed");
}

void Protocol::sump_reset() {
    for (uint i = 0; i < STAGE_COUNT; i++) {
        capture.get_config()->stages[i].mask = 0;
        capture.get_config()->stages[i].values = 0;
        capture.get_config()->stages[i].configuration = 0;
    }
}

inline void Protocol::put_str(const char* s) {
    int i = 0;
    while (s[i] != '\0') {
        uart->putc(s[i]);
        i++;
    }
}

inline void Protocol::put_uint32(uint32_t i) {
    uart->putc((char)i);
    uart->putc((char)(i >> 8));
    uart->putc((char)(i >> 16));
    uart->putc((char)(i >> 24));
}


// TODO check if adding timeout makes sense here or causes undefined behaviour
inline uint32_t Protocol::get_uint32() {
    uint32_t val = 0;
    for (int i=0; i < 4; i++) {
        task::sleep_us(1000);
        if (uart->available()) {
            val |= (uint32_t)uart->getc() << i*8;
        } else {
            val |= TIMEOUT << i*8;
        }
    }
    return val;
}