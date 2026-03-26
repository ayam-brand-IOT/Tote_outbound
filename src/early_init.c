// early_init.c
#include "esp_rom_gpio.h"
#include "hal/gpio_ll.h"

static const int DO_PINS[] = {35, 36, 37, 38, 39, 40};
#define DO_PIN_COUNT 6

volatile bool early_init_ran = false;  // flag compartido

void __attribute__((constructor)) early_gpio_init(void) {
    for (int i = 0; i < DO_PIN_COUNT; i++) {
        int pin = DO_PINS[i];
        esp_rom_gpio_pad_select_gpio(pin);
        gpio_ll_output_enable(&GPIO, pin);
        gpio_ll_set_level(&GPIO, pin, 0);
    }
    early_init_ran = true;
}