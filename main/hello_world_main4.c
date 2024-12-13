#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/dac_oneshot.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"

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
#define SAMPLE_RATE 8000       // 8 kHz
#define RECORD_TIME 40         // 40 seconds
#define BUFFER_SIZE (SAMPLE_RATE * RECORD_TIME) // 320,000 samples

// PSRAM Buffer
uint8_t *sound_buffer = NULL;
unsigned int write_index = 0; // Circular buffer write position
unsigned int buffer_full = 0; // Flag indicating buffer has wrapped around

// ADC and DAC handles
adc_oneshot_unit_handle_t adc_handle;
dac_oneshot_handle_t dac_handle;

// Initialize hardware
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

    // Allocate buffer in PSRAM
    sound_buffer = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (sound_buffer == NULL) {
        printf("Failed to allocate PSRAM buffer\n");
        while (1);
    }
    memset(sound_buffer, 0, BUFFER_SIZE); // Initialize buffer to 0
}

// Record audio into the circular buffer
void record_audio() {
    gpio_set_level(LED_PIN_RECORD, 1); // Turn on recording LED

    printf("Recording started...\n");
    while (gpio_get_level(BUTTON_RECORD) == 1) {
        int adc_val;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_val));

        // Store ADC value in circular buffer
        sound_buffer[write_index] = adc_val >> 4; // Scale 12-bit to 8-bit
        write_index = (write_index + 1) % BUFFER_SIZE;

        // Mark buffer as full if we wrap around
        if (write_index == 0) {
            buffer_full = 1;
        }

        esp_rom_delay_us(125); // 8 kHz sampling rate
    }

    printf("Recording stopped. Buffer is %s.\n", buffer_full ? "full" : "partially filled");
    gpio_set_level(LED_PIN_RECORD, 0); // Turn off recording LED
}

// Playback audio from the buffer
void playback_audio() {
    gpio_set_level(LED_PIN_PLAYBACK, 1); // Turn on playback LED

    printf("Playback started...\n");

    // Determine the starting point based on buffer status
    unsigned int read_index = buffer_full ? write_index : 0;
    unsigned int samples_to_play = buffer_full ? BUFFER_SIZE : write_index;

    for (unsigned int i = 0; i < samples_to_play; i++) {
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(dac_handle, sound_buffer[read_index]));
        read_index = (read_index + 1) % BUFFER_SIZE;
        esp_rom_delay_us(125); // Match playback rate to recording rate
    }

    printf("Playback finished.\n");
    gpio_set_level(LED_PIN_PLAYBACK, 0); // Turn off playback LED
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

