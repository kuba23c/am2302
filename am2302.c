#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/pio.h"

#include "am2302.pio.h"

#define AM2302_PIO pio0
#define AM2302_PIO_SM 0
#define AM2302_PIN 15
#define AM2302_DMA_CHANNEL 0
#define AM2302_PERIOD_MS 2000

#define MAIN_TASK_DELAY_MS 250

typedef struct
{
    uint8_t buffer[AM2302_BYTES_LEN];
    uint8_t cnt;
    bool dma_finished;
    bool pio_finished;
    bool error;
    bool init;
    bool started;
} am2302_t;

volatile am2302_t am2302 = {0};

static void am2302_dma_handler(void)
{
    am2302_dma_irq_clear(AM2302_DMA_CHANNEL);
    am2302.dma_finished = true;
}

static void am2302_pio_handler(void)
{
    am2302.pio_finished = true;
    am2302.error = !am2302.dma_finished;
    am2302_pio_irq_clear(AM2302_PIO);
}

static void ma2302_task(void)
{
    if (am2302.cnt >= AM2302_PERIOD_MS / MAIN_TASK_DELAY_MS)
    {
        am2302.cnt = 0;
        if (am2302.started)
        {
            am2302.dma_finished = false;
            am2302.pio_finished = false;
            am2302.error = false;

            // print all data
            printf("Read data:");
            for (uint8_t i = 0; i < AM2302_BYTES_LEN; ++i)
            {
                printf("%02X.", am2302.buffer[i]);
            }
            printf("\r\n");

            // check if valid
            uint32_t sum = 0;
            for (uint8_t i = 0; i < AM2302_BYTES_LEN - 1; ++i)
            {
                sum += am2302.buffer[i];
            }
            if ((sum & 0xFF) == am2302.buffer[AM2302_BYTES_LEN - 1])
            {
                printf("Data valid :D\r\n");
            }
            else
            {
                printf("Data corrupted :(\r\n");
            }

            // print humidity
            uint16_t humidity = am2302.buffer[0] << 8 | am2302.buffer[1];
            printf("Humidity: %d.%d\r\n", humidity / 10, humidity % 10);

            // print temperature
            uint16_t temperature = am2302.buffer[2] << 8 | am2302.buffer[3];
            printf("Temperature: %d.%d\r\n", temperature / 10, temperature % 10);

            printf("\r\n");
            memset((uint8_t *)am2302.buffer, 0, AM2302_BYTES_LEN);
        }
        am2302.started = true;
        printf("am2302 start\r\n");
        am2302_start(AM2302_PIO, AM2302_PIO_SM, (uint8_t *)am2302.buffer, AM2302_DMA_CHANNEL);
    }
    else
    {
        am2302.cnt++;
    }
}

int main()
{
    stdio_init_all();
    sleep_ms(100);

    printf("Hello, world!\r\n");

    am2302.init = !am2302_init(AM2302_PIO, AM2302_PIO_SM, AM2302_PIN, AM2302_DMA_CHANNEL, (uint8_t *)am2302.buffer, AM2302_BYTES_LEN, am2302_pio_handler, am2302_dma_handler);
    if (am2302.init)
    {
        printf("PIO init success :D\r\n");
    }
    else
    {
        printf("PIO init failed :(\r\n)");
    }

    gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, true);
    bool pin_value = false;
    while (true)
    {
        sleep_ms(MAIN_TASK_DELAY_MS);
        pin_value = !pin_value;
        gpio_put(PICO_DEFAULT_LED_PIN, pin_value);
        ma2302_task();
    }
}
