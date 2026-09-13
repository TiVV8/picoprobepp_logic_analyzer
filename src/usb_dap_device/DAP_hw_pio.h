///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// DAP HW implementation for the RP2040 MCU using
// a PIO program for increased performance. See
// DAP_hw_interface.h for more information.
// On RR2350 A2, this code seems not to run due to the E9 bug.
// Tests with the A4 version are planned.
//
#ifndef DAP_HW_PIO_H
#define DAP_HW_PIO_H

#include <cassert>
#include "config.h"
#include "DAP_hw_interface.h"
#include "DAP_log.h"
#include "dap_control.pio.h"
#include "task.h"
using namespace _SIO_;
using namespace _IO_BANK0_;

class DAP_hw_pio : public DAP_hw_interface {
public:

    explicit DAP_hw_pio()
    : _swdio_tms(GPIO_SWDIO_TMS), _swclk_tck(GPIO_SWCLK_TCK),
      _tdi(GPIO_TDI), _tdo(GPIO_TDO), _reset(GPIO_RESET) {
        // Load PIO program for low level DAP control
        _pio_dap = pio_rp2xxx::pio1.loadProgram(dap_control_program);
    }

    ///////////////////////////////
    // Timing configuration methods
    ///////////////////////////////

    inline void frequency_set(uint32_t f) override {
        _pio_dap->setClock(SM_CYCLES_PER_DAP_CLK * f);
    }

    void delay_us(uint32_t us) override {
        task::sleep_us(us);
    }

    constexpr bool test_domain_timer_support() override {
        return true;
    }
    inline uint32_t test_domain_timer_frequency() override {
        return 1000000;
    }
    inline uint32_t test_domain_timer_get() override {
        return task::micros();
    }

    ////////////////////////////
    // Pin configuration methods
    ////////////////////////////

    void connect_jtag_pins() override {
        DAP_LOG(DAP_log::LOG_INFO, "connect_jtag_pins()");
        _swdio_tms.gpioMode(GPIO::INPUT | GPIO::OUTPUT | GPIO::FAST | GPIO::INIT_LOW);
        _swclk_tck.gpioMode(GPIO::INPUT | GPIO::OUTPUT | GPIO::FAST);
        _tdi.gpioMode      (GPIO::INPUT | GPIO::OUTPUT | GPIO::FAST);
        _tdo.gpioMode      (GPIO::INPUT);
        _reset.gpioMode    (GPIO::INPUT | GPIO::OUTPUT | GPIO::INIT_HIGH);

        _swdio_tms.setSEL(GPIO_CTRL_FUNCSEL__pio1);
        _swclk_tck.setSEL(GPIO_CTRL_FUNCSEL__pio1);
        _tdi.setSEL(GPIO_CTRL_FUNCSEL__pio1);
        _tdo.setSEL(GPIO_CTRL_FUNCSEL__pio1);

        _pio_dap->disable();
        configure_JTAG(_pio_dap, GPIO_SWDIO_TMS, GPIO_SWCLK_TCK, GPIO_TDI, GPIO_TDO);
        _pio_dap->enable();
    }

    void connect_swd_pins() override {
        DAP_LOG(DAP_log::LOG_INFO, "connect_swd_pins()");
        _swclk_tck.gpioMode(GPIO::INPUT | GPIO::OUTPUT | GPIO::FAST);
        _swdio_tms.gpioMode(GPIO::INPUT | GPIO::OUTPUT | GPIO::FAST);

        _swclk_tck.setSEL(GPIO_CTRL_FUNCSEL__pio1);
        _swdio_tms.setSEL(GPIO_CTRL_FUNCSEL__pio1);

        _pio_dap->disable();
        configure_SWD(_pio_dap, GPIO_SWDIO_TMS, GPIO_SWCLK_TCK);
        _pio_dap->enable();
    }

    void disconnect() override {
        DAP_LOG(DAP_log::LOG_INFO, "disconnect()");
        _swclk_tck.gpioMode(GPIO::INPUT);
        _swdio_tms.gpioMode(GPIO::INPUT);
        _tdi.gpioMode(GPIO::INPUT);
        _tdo.gpioMode(GPIO::INPUT);

        _swclk_tck.setSEL(GPIO_CTRL_FUNCSEL__null);
        _swdio_tms.setSEL(GPIO_CTRL_FUNCSEL__null);
        _tdi.setSEL(GPIO_CTRL_FUNCSEL__null);
        _tdo.setSEL(GPIO_CTRL_FUNCSEL__null);
    }

    ///////////////////////////
    // Common operation methods
    ///////////////////////////

    inline void clock_cycle(uint16_t len) override {
        if (len == 0) return;
        _cmd.len = len-1; // PIO code expects len-1
        _cmd.cmd = dap_control_offset_cycle + _pio_dap->load_addr;
        _pio_dap->writeTxFifo(_cmd.value);
    }

    /////////////////////////
    // SWD read write methods
    /////////////////////////

    inline uint32_t swd_read(uint8_t len) override {
        swd_swdio_enable_output(false);
        _cmd.len = len-1; // PIO code expects len-1
        _cmd.cmd = dap_control_offset_read + _pio_dap->load_addr;
        _pio_dap->writeTxFifo(_cmd.value);
        uint32_t res = _pio_dap->readRxFifo();
        res >>= (32-len);
        return res;
    }

    inline void swd_write(uint32_t val, uint8_t len) override {
        swd_swdio_enable_output(true);
        _cmd.len = len-1; // PIO code expects len-1
        _cmd.cmd = dap_control_offset_write + _pio_dap->load_addr;
        _pio_dap->writeTxFifo(_cmd.value);
        _pio_dap->writeTxFifo(val);
    }

    inline void swd_swdio_enable_output(bool b) override {
        static bool last_oe = true;
        if (b != last_oe) {
            _cmd.len = 0;
            _cmd.cmd = dap_control_offset_execute + _pio_dap->load_addr;
            _pio_dap->writeTxFifo(_cmd.value);
            _pio_dap->writeTxFifo(op_SET(0, set_dest_t::PINDIRS, b));
            last_oe = b;
        }
    }

    //////////////////////////
    // JTAG read write methods
    //////////////////////////

    inline uint32_t jtag_read(uint8_t len) override {
        _cmd.len = len-1; // PIO code expects len-1
        _cmd.cmd = dap_control_offset_read + _pio_dap->load_addr;
        _pio_dap->writeTxFifo(_cmd.value);
        uint32_t res = _pio_dap->readRxFifo();
        res >>= (32-len);
        return res;
    }

    inline uint32_t jtag_write(uint32_t val, uint8_t len) override {
        _cmd.len = len-1; // PIO code expects len-1
        _cmd.cmd = dap_control_offset_write + _pio_dap->load_addr;
        _pio_dap->writeTxFifo(_cmd.value);
        _pio_dap->writeTxFifo(val);
        return val >> len;
    }

    inline uint32_t jtag_read_write(uint32_t val, uint8_t len) override {
        _cmd.len = len-1; // PIO code expects len-1
        _cmd.cmd = dap_control_offset_read_write + _pio_dap->load_addr;
        _pio_dap->writeTxFifo(_cmd.value);
        _pio_dap->writeTxFifo(val);
        uint32_t res = _pio_dap->readRxFifo();
        res >>= (32-len);
        return res;
    }

    /////////////////////////////
    // Direct SWD/JTAG Pin access
    /////////////////////////////

    inline void swdio_tms_set(bool v) override {
        static bool last_tms = LOW;
        if (v != last_tms) {
            _cmd.len = 0;
            _cmd.cmd = dap_control_offset_execute + _pio_dap->load_addr;
            _pio_dap->writeTxFifo(_cmd.value);
            _pio_dap->writeTxFifo(op_SET(0, set_dest_t::PINS, v));
            last_tms = v;
        }
    }

    inline bool swdio_tms_get() override {
        return SIO.GPIO_IN & (1 << GPIO_SWDIO_TMS);
    }

    inline void swclk_tck_set(bool) override {
        // We ignore manual setting of SWCLK.
        // Such requests might come from the
        // SWJ_Pins command, but PIO is in charge
        // of the SWCLK pin...
    }

    inline bool swclk_tck_get() override {
        return SIO.GPIO_IN & (1 << GPIO_SWCLK_TCK);
    }

    inline void tdi_set(bool) override {
    }

    inline bool tdi_get() override {
        return SIO.GPIO_IN & (1 << GPIO_TDI);
    }

    inline bool tdo_get() override {
        return SIO.GPIO_IN & (1 << GPIO_TDO);
    }

    inline void trst_set(bool) override {
    }

    inline bool trst_get() override {
        return false;
    }

    inline bool reset_set(bool) override {
        return false;
    }

    inline bool reset_get() override {
        return false;
    }

private:
    gpio_rp2xxx         _swdio_tms;
    gpio_rp2xxx         _swclk_tck;
    gpio_rp2xxx         _tdi;
    gpio_rp2xxx         _tdo;
    gpio_rp2xxx         _reset;

    std::unique_ptr<SM> _pio_dap;
    swd_cmd_t           _cmd;
};

#endif // DAP_HW_PIO_H
