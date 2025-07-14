#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "string.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "epd/Dev_Config.h"
#include "epd/EPD_7in5b_V2.h"
#include "Dev_Config.h"

static int contains(const char *text, const char *words) {
    if (strlen(words) > strlen(text)) return false;

    int j = 0;

    for (int i = 0; i < strlen(text); i++) {
        if (text[i] == words[j]) {
            if (j == strlen(words) - 1) return true;
            j++;
        }
        else j = 0;
    }

    return false;
} 

//
// PICO 1
//

int main()
{
    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    sleep_ms(7000);

    printf("Starting\n");

    DEV_GPIO_Init();

    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1000);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
    } else {
        printf("Clean boot\n");
    }
    // watchdog_enable(10000, 1);

    EPD_7IN5B_V2_Init();
    EPD_7IN5B_V2_Clear();

    DEV_Delay_ms(500);

    while (true) {    
        // watchdog_update();   
        
        printf("LOOPING\n");

        sleep_ms(1000);
    }
}
