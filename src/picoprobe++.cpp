///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// picoprobe++ main code. Not so much to do - pick up all the
// various pieces (DAP interface, CDC ACM UART), and tie them
// together. Sign up all tasks and start the multitasking kernel.
// The USB configuration is mainly controlled by the configuration
// file, depending on the MCU board.
//
#include "config.h"

#include "usb_bos.h"
#include "usb_dcd.h"
#include "usb_device.h"
#include "usb_device_controller.h"

#include "usb_ms_OS_20_capability.h"
#include "usb_ms_func_subset.h"
#include "usb_ms_compatible_ID.h"
#include "usb_ms_registry_property.h"
using enum TUPP::wPropertyDataType_t;

#include "usb_dap_device.h"
#include "usb_uart_device.h"
#include "usb_cdc_acm_adapter.h"
#include "DAP_hw_pio.h"
#include "DAP_hw_gpio.h"
#include "DAP_log.h"
#include "LA_log.h"
#include "Logic_Analyzer.h"

#include "posix_io.h"
#include "task.h"
#include "task_monitor.h"

int main() {
    // Optional: Firmware Debug UART
    #ifdef DEBUG_UART_ENABLE
    uart_rp2xxx uart(DEBUG_UART_TX_GPIO, DEBUG_UART_RX_GPIO);
    posix_io::inst.register_stdio(uart);
    #endif

    // Logging control for USB, DAP code and Logic Analyzer
    usb_log::inst.setLevel(usb_log::DEBUG_LEVEL_USB);
    DAP_log::inst.setLevel(DAP_log::DEBUG_LEVEL_DAP);
    LA_log::inst.setLevel(DAP_log::DEBUG_LEVEL_LA);

    // Get the unique ID of this MCU/board
    auto id = unique_id_rp2xxx::read_unique_id_string();

    // Set up USB Device driver, USB device and
    // generic device controller on top
    usb_dcd & driver = usb_dcd::inst();
    usb_device device;
    usb_device_controller controller(driver, device);

    // USB device descriptor
    device.set_bcdUSB          (USB_DEV_bcdUSB);
    device.set_bMaxPacketSize0 (USB_DEV_bMaxPacketSize0);
    device.set_idVendor        (USB_DEV_VID);
    device.set_idProduct       (USB_DEV_PID);
    device.set_Manufacturer    (USB_DEV_Manufacturer);
    device.set_Product         (USB_DEV_Product);
    device.set_SerialNumber    (id.data());
    device.set_bcdDevice       (USB_DEV_bcdDevice);

    // USB configuration descriptor
    usb_configuration config(device);
    config.set_bConfigurationValue( 1 );
    config.set_bmAttributes({ .remote_wakeup = 0,
                              .self_powered  = 0,
                              .bus_powered   = 1});
    config.set_bMaxPower_mA(100);

    // Set up CMSIS DAP device. Select DAP HW driver and create
    // a DAP protocol instance using this driver. Then use the
    // DAP protocol instance for the USB DAP device, which will
    // handle the USB bulk endpoints amd process the CMSIS DAP
    // requests.
    #ifdef DAP_USE_GPIO_BACKEND
    DAP_hw_gpio dap_hw; // use GPIO implementation
    #else
    DAP_hw_pio dap_hw; // use PIO implementation
    #endif
    DAP_Protocol dap(dap_hw);
    dap.set_serial(id.data());
    usb_dap_device dap_device(controller, config, dap);
    dap_device.sign_up();
    dap_device.setPriority(100);

    // Set up CDC ACM device to target. Create a HW UART object
    // and forward it to the USB UART device.
    uart_rp2xxx target_uart(UART_TARGET_TX_GPIO, UART_TARGET_RX_GPIO);
    usb_uart_device target_uart_device(controller, config, target_uart);
    target_uart_device.set_FunctionName("Target debug UART");
    target_uart_device.sign_up();
    target_uart_device.setPriority(90);

    // Optional: Set up another CDC ACM device for debugging the debugger
    // firmware (connected to stdio of the debugger SW).
    #ifdef DEBUG_USB_UART_ENABLE
    usb_cdc_acm_adapter usb_uart_adapter(controller, config);
    usb_uart_adapter.set_FunctionName("Firmware debug UART");
    posix_io::inst.register_stdio(usb_uart_adapter);
    #endif

    // Add BOS and MS OS 2.0 capability descriptor
    usb_bos bos(controller, device); // Add a Binary Object Store
    usb_ms_OS_20_capability ms_os20(bos);

    // This functional subset is only valid for a
    // specific interface number, here the DAP interface
    usb_ms_func_subset ms_func_subset(ms_os20.header);
    ms_func_subset.set_bFirstInterface(
            dap_device.interface_dap.descriptor.bInterfaceNumber);

    // The DAP interface will use the WINUSB driver in Windows
    usb_ms_compatible_ID compat_id(ms_func_subset);
    compat_id.set_compatible_id( "WINUSB" );

    // This GUID specifies a CMSIS-DAP v2 interfaces
    usb_ms_registry_property reg_prop(ms_func_subset);
    reg_prop.set_wPropertyDataType(REG_MULTI_SZ);
    reg_prop.add_string( "DeviceInterfaceGUIDs" );
    reg_prop.add_string( "{CDB3B5AD-293B-4663-AA36-1AAE46463776}" );
    reg_prop.add_end_marker();

    // LED handler - connect USB/DAP callbacks to the LEDs
    DAP_led leds;
    usb_uart_device::uart_tx_cb = [&]()       { leds.trigger_uart_tx_led(); };
    usb_uart_device::uart_rx_cb = [&]()       { leds.trigger_uart_rx_led(); };
    DAP_Protocol::connected_cb  = [&](bool v) { leds.set_connected_led(v);  };
    DAP_Protocol::running_cb    = [&](bool v) { leds.set_running_led(v);    };

    // Set up Logic Analyzer
    usb_cdc_acm_adapter uart(controller, config);
    uart.set_FunctionName("Picoprobe++ Logic Analyzer UART");

    Logic_Analyzer la(uart);
    la.sign_up();
    la.setPriority(80);

    // Optional: YAHAL Task Monitor
    #ifdef START_TASK_MONITOR
    task_monitor monitor;
    monitor.sign_up();
    monitor.setPriority(5);
    #endif

    // Activate the USB device
    driver.pullup_enable(true);

    // Wait until USB enumeration has finished
    TUPP_LOG(LOG_INFO, "Waiting for USB connection ...");
    while (!controller.active_configuration) {
        task::sleep_ms(20);
    }

    // Set DCD and DTR after USB has connected
    target_uart_device.set_dcd_dtr(true, true);

    // Welcome the user by blinking some :)
    // This also indicates that the USB enumeration
    // worked and the probe is ready to be used!
    leds.welcome();

    TUPP_LOG(LOG_INFO, "Starting the scheduler ...");
    // Start the multitasking kernel
    task::start_scheduler();
}
