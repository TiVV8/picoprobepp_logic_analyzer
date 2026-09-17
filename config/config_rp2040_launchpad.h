///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// Debugger configuration file for the RP2040 Launchpad
//
#ifndef CONFIG_RP2040_LAUNCHPAD_H
#define CONFIG_RP2040_LAUNCHPAD_H

#include "version.h"
#include "usb_config.h"

////////////////////////////////
// Firmware Debugging Options //
////////////////////////////////

#define DEBUG_UART_ENABLE                       // Enable debug output via UART
#define DEBUG_UART_TX_GPIO          BC_UART_TX
#define DEBUG_UART_RX_GPIO          BC_UART_RX
//#define DEBUG_USB_UART_ENABLE                   // Enable debug output via separate CDC ACM device
#define DEBUG_LEVEL_USB	            LOG_INFO
#define DEBUG_LEVEL_DAP             LOG_INFO
#define DEBUG_LEVEL_LA             LOG_INFO
//#define START_TASK_MONITOR                      // Start the task monitor

///////////////////////
// USB configuration //
///////////////////////

// Definition needed for openDAP++
#define USB_DEFAULT_PAKET_SIZE      TUPP_DEFAULT_PAKET_SIZE

// USB device configuration
#define USB_DEV_bcdUSB              0x210       // USB version in BCD
#define USB_DEV_bMaxPacketSize0     TUPP_DEFAULT_PAKET_SIZE
#define USB_DEV_VID                 0x2e8a      // Raspberry Pi (Trading) Limited
#define USB_DEV_PID                 0x1089      // RP2040 Lauchpad (see https://github.com/raspberrypi/usb-pid)
#define USB_DEV_Manufacturer        "FH Aachen"
#define USB_DEV_Product             "RP2xxx Lauchpad Debug Probe (CMSIS-DAP)"
#define USB_DEV_bcdDevice           VERSION_BCD // Product version

///////////////////////
// DAP configuration //
///////////////////////

// CMSIS DAP configuration
#define DAP_PROTOCOL_VERSION        "2.1.1"
//#define DAP_USE_GPIO_BACKEND

// Vendor Name and Product Name. If these values are set to empty
// strings, the USB Device Information is used to obtain these values!
#define DAP_VENDOR_NAME             USB_DEV_Manufacturer
#define DAP_PRODUCT_NAME            USB_DEV_Product
#define DAP_FIRMWARE_VERSION        VERSION_STRING

// Device/Board information if debug probe is fixed to a dev board.
// Use empty strings if the chip device and board are not know.
// This configuration assumes a RP2350 Launchpad...
#define DAP_DEVICE_VENDOR           "Raspberry Pi"
#define DAP_DEVICE_NAME             "RP2350"
#define DAP_BOARD_VENDOR            "FH Aachen"
#define DAP_BOARD_NAME              "RP2350 Launchpad"

// Maximum Package Buffers for Command and Response data.
// This configuration settings is used to optimize the communication
// performance with the debugger and depends on the USB peripheral.
#define DAP_MAX_PACKET_COUNT        10

// Maximum Package Size for Command and Response data.
// This configuration settings is used to optimize the communication
// performance with the debugger and depends on the USB peripheral.
// Typical vales are 64 for Full-speed USB HID or WinUSB,
// 1024 for High-speed USB HID and 512 for High-speed USB WinUSB.
#define DAP_MAX_PACKET_SIZE         USB_DEFAULT_PAKET_SIZE

// CMSIS DAP Protocol support
#define DAP_CAP_SWD_SUPPORT         true
#define DAP_CAP_JTAG_SUPPORT        true
#define DAP_DEFAULT_PORT            PORT_SWD
#define DAP_DEFAULT_CLOCK           1000000

// SWO settings
#define DAP_CAP_SWO_UART            false
#define DAP_CAP_SWO_MANCHESTER      false
#define DAP_CAP_SWO_STREAMING_TRACE false
#define DAP_SWO_BUFFER_SIZE         0

// DAP UART settings
#define DAP_CAP_UART_COM_PORT       false
#define DAP_CAP_USB_COM_PORT        false
#define DAP_UART_RX_SIZE            0
#define DAP_UART_TX_SIZE            0

#define JTAG_DEV_COUNT              8
#define JTAG_IR_LENGTH              4


//////////////////////////////////
// Logic Analyzer configuration //
//////////////////////////////////

// SUMP metdata
#define DEVICE_NAME "RP2040 Launchpad Debug Probe"
#define DEVICE_VERSION "v0.1"
#define MAX_CHANNELS 12
#define MAX_TOTAL_SAMPLES 200000  // in bytes (TODO check for Debug Probe)
#define MAX_SAMPLE_RATE 200000000  // Hz | Maximum rate for sump protocol
#define CLOCK_RATE 200000000  // Hz | Required because clock divisor by libsigrok, is based on this value
#define PROTOCOL_VERSION 2

////////////////////////
// GPIO configuration //
////////////////////////

// SWD/JTAG Hardware configuration
#define GPIO_SWCLK_TCK              6
#define GPIO_SWDIO_TMS              7
#define GPIO_RESET                  15
#define GPIO_TDI                    16
#define GPIO_TDO                    17

// USB UART configuration
#define UART_TARGET_TX_GPIO         8           // probe to target
#define UART_TARGET_RX_GPIO         9           // target to probe
#define UART_TARGET_BUFFER_SIZE     1024

#endif // CONFIG_RP2040_LAUNCHPAD_H

