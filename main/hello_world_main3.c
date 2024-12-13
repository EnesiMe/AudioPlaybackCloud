#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/dac_oneshot.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// ADC and DAC settings
#define ADC_CHANNEL ADC_CHANNEL_0
#define ADC_WIDTH ADC_BITWIDTH_12
#define DAC_CHANNEL DAC_CHAN_0

// GPIO settings
#define LED_PIN_RECORD GPIO_NUM_2
#define LED_PIN_PLAYBACK GPIO_NUM_4
#define BUTTON_PLAY GPIO_NUM_33
#define BUTTON_RECORD GPIO_NUM_13

// Buffer settings
#define BUFFER_SIZE 80000 // For 10 seconds at 8 kHz
uint8_t sound_array[BUFFER_SIZE];
unsigned int buffer_index = 0;

// Handles
adc_oneshot_unit_handle_t adc_handle;
dac_oneshot_handle_t dac_handle;

// Initialize ADC and DAC
static void init_hw(void) {
    // GPIO setup
    gpio_set_direction(LED_PIN_RECORD, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIN_PLAYBACK, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_PLAY, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PLAY, GPIO_PULLDOWN_ONLY);
    gpio_set_direction(BUTTON_RECORD, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_RECORD, GPIO_PULLDOWN_ONLY);

    // ADC setup
    adc_oneshot_unit_init_cfg_t adc_cfg = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t adc_channel_cfg = {
        .bitwidth = ADC_WIDTH,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &adc_channel_cfg));

    // DAC setup
    dac_oneshot_config_t dac_cfg = {.chan_id = DAC_CHANNEL};
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&dac_cfg, &dac_handle));
}

// Record audio
void record_audio() {
    gpio_set_level(LED_PIN_RECORD, 1);
    buffer_index = 0;

    printf("Recording started...\n");

    while (gpio_get_level(BUTTON_RECORD) == 1 && buffer_index < BUFFER_SIZE) {
        int adc_val;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_val));
        sound_array[buffer_index++] = adc_val >> 4; // Scale 12-bit ADC to 8-bit
        esp_rom_delay_us(125); // 8 kHz sampling rate (125 µs interval)
    }

    printf("Recording stopped. Recorded %u samples.\n", buffer_index);
    gpio_set_level(LED_PIN_RECORD, 0);
}

// Playback audio
void playback_audio() {
    gpio_set_level(LED_PIN_PLAYBACK, 1);

    printf("Playback started...\n");

    for (unsigned int i = 0; i < buffer_index; i++) {
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(dac_handle, sound_array[i]));
        esp_rom_delay_us(125); // Match playback rate to recording rate
    }

    printf("Playback finished.\n");
    gpio_set_level(LED_PIN_PLAYBACK, 0);
}

// Main application
void app_main() {
    init_hw();

    printf("Press RECORD (GPIO13) to record audio. Press PLAY (GPIO33) to play it back.\n");

    while (1) {
        if (gpio_get_level(BUTTON_RECORD) == 1) {
            record_audio();
        }
        if (gpio_get_level(BUTTON_PLAY) == 1) {
            playback_audio();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

