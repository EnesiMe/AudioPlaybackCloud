#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_http_client.h"

#define SAMPLE_RATE 16000 // 16kHz
#define AUDIO_DURATION 20 // 20 seconds
#define BUFFER_SIZE (SAMPLE_RATE * AUDIO_DURATION)
#define ADC_CHANNEL ADC1_CHANNEL_0 // GPIO36
#define DAC_CHANNEL DAC_CHANNEL_1 // DAC_OUT1 on GPIO25

// Pin Definitions
#define RECORD_BUTTON_PIN GPIO_NUM_13
#define PLAYBACK_BUTTON_PIN GPIO_NUM_33
#define RECORD_LED_PIN GPIO_NUM_2
#define PLAYBACK_LED_PIN GPIO_NUM_4

// Global Variables
static int16_t audio_buffer[BUFFER_SIZE];
static size_t write_index = 0;
static bool is_recording = false;
static const char *TAG = "AudioReplay";

// Function Prototypes
void record_audio_task(void *arg);
void playback_audio_task(void *arg);
void upload_audio_to_cloud(int16_t *audio_data, size_t size);
void adc_init();
void dac_init();
void gpio_init();

void app_main() {
    gpio_init();
    adc_init();
    dac_init();

    // Create tasks for recording and playback
    xTaskCreate(record_audio_task, "record_audio_task", 4096, NULL, 5, NULL);
    xTaskCreate(playback_audio_task, "playback_audio_task", 4096, NULL, 5, NULL);
}

// GPIO Initialization
void gpio_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RECORD_BUTTON_PIN) | (1ULL << PLAYBACK_BUTTON_PIN) |
                        (1ULL << RECORD_LED_PIN) | (1ULL << PLAYBACK_LED_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Initialize LEDs as OFF
    gpio_set_level(RECORD_LED_PIN, 0);
    gpio_set_level(PLAYBACK_LED_PIN, 0);
}

// ADC Initialization
void adc_init() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);
}

// DAC Initialization
void dac_init() {
    dac_output_enable(DAC_CHANNEL);
}

// Task to record audio
void record_audio_task(void *arg) {
    while (1) {
        if (gpio_get_level(RECORD_BUTTON_PIN) == 0) { // Button pressed
            gpio_set_level(RECORD_LED_PIN, 1);
            is_recording = true;

            ESP_LOGI(TAG, "Recording started...");
            while (gpio_get_level(RECORD_BUTTON_PIN) == 0) {
                int16_t sample = adc1_get_raw(ADC_CHANNEL);
                audio_buffer[write_index] = sample;
                write_index = (write_index + 1) % BUFFER_SIZE;
                vTaskDelay(pdMS_TO_TICKS(1000 / SAMPLE_RATE));
            }

            gpio_set_level(RECORD_LED_PIN, 0);
            is_recording = false;

            ESP_LOGI(TAG, "Recording stopped. Uploading to cloud...");
            upload_audio_to_cloud(audio_buffer, BUFFER_SIZE);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task to playback audio
void playback_audio_task(void *arg) {
    while (1) {
        if (gpio_get_level(PLAYBACK_BUTTON_PIN) == 0) { // Button pressed
            gpio_set_level(PLAYBACK_LED_PIN, 1);

            ESP_LOGI(TAG, "Playing back the last 20 seconds of audio...");
            for (size_t i = 0; i < BUFFER_SIZE; ++i) {
                dac_output_voltage(DAC_CHANNEL, audio_buffer[(write_index + i) % BUFFER_SIZE] >> 4);
                vTaskDelay(pdMS_TO_TICKS(1000 / SAMPLE_RATE));
            }

            gpio_set_level(PLAYBACK_LED_PIN, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Function to upload audio to Google Cloud
void upload_audio_to_cloud(int16_t *audio_data, size_t size) {
    char *base64_audio = calloc(1, size * 2); // Adjust buffer size as needed
    size_t out_len = 0;

    // Base64 encode audio
    esp_base64_encode((const unsigned char *)audio_data, size * sizeof(int16_t), base64_audio, &out_len);

    esp_http_client_config_t config = {
        .url = "https://script.google.com/macros/s/AKfycbyJEgBu_YFu2q3H1gXqBydhXsCowDv1_UZp3jGs2s8dtSWfsW6i6aoGMU4ceR-Xvx02/exec",
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    char json_payload[1024];
    snprintf(json_payload, sizeof(json_payload), "{\"audio\":\"%s\"}", base64_audio);
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Audio uploaded successfully");
    } else {
        ESP_LOGE(TAG, "Failed to upload audio: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(base64_audio);
}

