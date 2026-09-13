#include "usb_dcd.h"
#include "usb_device.h"
#include "usb_device_controller.h"
#include "usb_cdc_acm_adapter.h"
#include "usb_bos.h"
#include "usb_ms_OS_20_capability.h"
#include "task.h"
#include "LA_log.h"

#include "config.h"
#include "unique_id_rp2040.h"
#define   unique_id_rp2xxx unique_id_rp2040
#include "uart_rp2040.h"
#define uart_rp2xxx uart_rp2040

#include "protocol.h"

class Logic_Analyzer : public task {
public:
    Logic_Analyzer(uart_data_interface& uart) : task("Logic Analyzer task"), protocol(uart) {}

    [[noreturn]] void run() override {
        protocol.start();
    }

private:
    Protocol protocol;
};

int main() {
    usb_dcd & driver = usb_dcd::inst();
    usb_device device;
    usb_device_controller controller(driver, device);

    // auto id = unique_id_rp2xxx::read_unique_id_string();

    // USB device descriptor
    device.set_bcdUSB          (USB_DEV_bcdUSB);
    device.set_bMaxPacketSize0 (USB_DEV_bMaxPacketSize0);
    device.set_idVendor        (USB_DEV_VID);
    device.set_idProduct       (USB_DEV_PID);
    device.set_Manufacturer    (USB_DEV_Manufacturer);
    device.set_Product         (USB_DEV_Product);
    device.set_SerialNumber    (unique_id_rp2xxx::read_unique_id_string().data());
    device.set_bcdDevice       (USB_DEV_bcdDevice);

    // USB configuration descriptor
    usb_configuration config(device);
    config.set_bConfigurationValue( 1 );
    config.set_bmAttributes({ .remote_wakeup = 0,
                            .self_powered  = 0,
                            .bus_powered   = 1});
    config.set_bMaxPower_mA(100);

    usb_cdc_acm_adapter uart(controller, config);
    uart.set_FunctionName("Picoprobe++ Logic Analyzer UART");

    // DEBUG
    usb_cdc_acm_adapter usb_uart_adapter(controller, config);
    usb_uart_adapter.set_FunctionName("Firmware debug UART");
    posix_io::inst.register_stdio(usb_uart_adapter);
    LA_log::inst.setLevel(LA_log::LOG_DEBUG);
    //

    // Add BOS and MS OS 2.0 capability descriptor
    usb_bos bos(controller, device); // Add a Binary Object Store
    usb_ms_OS_20_capability ms_os20(bos);
    // Activate the USB device
    driver.pullup_enable(true);

    // Wait until USB enumeration has finished
    while (!controller.active_configuration) {
        task::sleep_ms(20);
    }

    //debugger set log level

    Logic_Analyzer la(uart);
    la.run();
}