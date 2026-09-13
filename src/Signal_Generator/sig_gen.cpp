#define RP2350


#include "gpio_rp2350.h"
#include "task.h"
#include "board.h"

int main() {
    // LED
    gpio_rp2350 led(LED_RED_GPIO);
    led.gpioMode(GPIO::OUTPUT | GPIO::INIT_LOW);
    // GPIO
    gpio_rp2350 gpio(23);
    gpio.setMode(GPIO::OUTPUT | GPIO::INIT_LOW);

    
    // Blink to signal program installation and start of signal generation
    for (int i=0; i < 4; i++) {
        led.gpioToggle();
        task::sleep_ms(500);
    }
    led.gpioToggle();
    task::sleep_ms(1000);
    led.gpioToggle();

    // Generate signals
    int i = 0;
    while (true) {
        task::sleep_ms(100);
        gpio.gpioToggle();
        //if (i < 10) led.gpioToggle();
        //i = (i + 1) % 600;
        led.gpioToggle();
    }
}