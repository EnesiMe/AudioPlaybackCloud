#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/dac_oneshot.h"
#include "driver/gpio.h"

// ADC and DAC settings
#define ADC_CHANNEL ADC_CHANNEL_0  // ADC channel 0 (GPIO36)
#define ADC_WIDTH ADC_BITWIDTH_12  // 12-bit resolution
#define DAC_CHANNEL DAC_CHAN_0     // DAC channel 0 (GPIO25)

// GPIO settings
#define LED_PIN_RECORD GPIO_NUM_2   // GPIO2 for recording indicator
#define LED_PIN_PLAYBACK GPIO_NUM_4 // GPIO4 for playback indicator
#define BUTTON_PLAY GPIO_NUM_33     // GPIO33 to trigger playback
#define BUTTON_RECORD GPIO_NUM_13   // GPIO13 to trigger recording

// Buffer settings
#define BUFFER_SIZE 160000
char sound_array[BUFFER_SIZE]; // Buffer for audio data
unsigned int buffer_index = 0; // Current write position in the buffer

// ADC and DAC handles
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
    adc_oneshot_unit_init_cfg_t adc_init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t adc_channel_cfg = {
        .bitwidth = ADC_WIDTH,
        .atten = ADC_ATTEN_DB_12, // Equivalent to the previous 11dB, max input voltage: 3.6V
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &adc_channel_cfg));

    // DAC setup
    dac_oneshot_config_t dac_config = {
        .chan_id = DAC_CHANNEL,
    };
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&dac_config, &dac_handle));
}

// Record audio into the buffer
void record_audio() {
    gpio_set_level(LED_PIN_RECORD, 1); // Turn on recording LED
    buffer_index = 0;

    printf("Recording started...\n");
    while (gpio_get_level(BUTTON_RECORD) == 1 && buffer_index < BUFFER_SIZE) {
        int adc_val;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_val)); // Read ADC value
        sound_array[buffer_index++] = adc_val / 16; // Scale 12-bit ADC to 8-bit value
        vTaskDelay(pdMS_TO_TICKS(1)); // Simulate sampling rate (1ms delay ~1kHz sampling)
    }

    printf("Recording stopped. Recorded %u samples.\n", buffer_index);
    gpio_set_level(LED_PIN_RECORD, 0); // Turn off recording LED
}

// Playback audio from the buffer
void playback_audio() {
    gpio_set_level(LED_PIN_PLAYBACK, 1); // Turn on playback LED

    printf("Playback started...\n");
    for (unsigned int i = 0; i < buffer_index; i++) {
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(dac_handle, sound_array[i])); // Output audio
        vTaskDelay(pdMS_TO_TICKS(1)); // Simulate playback rate (1ms delay ~1kHz)
    }

    printf("Playback finished.\n");
    gpio_set_level(LED_PIN_PLAYBACK, 0); // Turn off playback LED
}

// Main application
void app_main() {
    init_hw(); // Initialize hardware

    printf("Press the record button (GPIO13) to start recording.\n");
    printf("Press the playback button (GPIO33) to play recorded audio.\n");

    while (1) {
        if (gpio_get_level(BUTTON_RECORD) == 1) {
            record_audio();
        }

        if (gpio_get_level(BUTTON_PLAY) == 1) {
            playback_audio();
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Polling delay
    }
}

