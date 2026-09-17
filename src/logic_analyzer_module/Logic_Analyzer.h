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
