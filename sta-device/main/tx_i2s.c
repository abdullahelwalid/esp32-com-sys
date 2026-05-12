#include "tx_i2s.h"

#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

/* Stream format (must match AP: mono int16 @ this rate over UDP). */
#define AUDIO_SAMPLE_RATE_HZ 16000

/* Digital gain before I2S (int16 → int32 line). Increase if still quiet; too high clips. */
#define PCM_DIGITAL_GAIN_NUM 3
#define PCM_DIGITAL_GAIN_DEN 1

/* MAX98357A I2S pins (must match your wiring). */
#define I2S_BCLK_GPIO  26
#define I2S_WS_GPIO    27
#define I2S_DOUT_GPIO  25

/*
 * MAX98357A SD (shutdown): tie to VIN on the module, or drive high from ESP32.
 * Set to a GPIO number to drive HIGH at init; set to -1 if SD is not wired to ESP32.
 */
#define I2S_AMP_SD_GPIO (-1)

/* 1 = play a short tone after init so you can hear if I2S + amp work before UDP. */
#define TX_I2S_PLAY_TEST_TONE 1

static const char *TAG = "tx_i2s";

static i2s_chan_handle_t s_tx_handle;

/* Worst-case UDP payload: 512 int16 mono → 512 stereo int32 frames */
#define TX_MAX_MONO_SAMPLES 512
static int32_t s_stereo_i32[TX_MAX_MONO_SAMPLES * 2];

static void amp_sd_init(void)
{
#if I2S_AMP_SD_GPIO >= 0
    gpio_reset_pin(I2S_AMP_SD_GPIO);
    gpio_set_direction(I2S_AMP_SD_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(I2S_AMP_SD_GPIO, 1);
    ESP_LOGI(TAG, "MAX98357A SD pin GPIO%d driven high (amp enabled)", I2S_AMP_SD_GPIO);
#endif
}

void tx_i2s_init(void)
{
    amp_sd_init();

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    /* Larger DMA ring reduces I2S underrun when Wi-Fi delivery jitters. */
    chan_cfg.dma_desc_num = 10;
    chan_cfg.dma_frame_num = 512;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx_handle, NULL));

    /* Same pattern as ESP-IDF i2s_std TX example: 32-bit stereo MSB. */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws = I2S_WS_GPIO,
            .dout = I2S_DOUT_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx_handle));

    ESP_LOGI(TAG, "I2S TX: %d Hz, 32-bit stereo MSB, BCLK=%d WS=%d DOUT=%d",
             AUDIO_SAMPLE_RATE_HZ, I2S_BCLK_GPIO, I2S_WS_GPIO, I2S_DOUT_GPIO);
}

void tx_i2s_play_test_tone(void)
{
#if TX_I2S_PLAY_TEST_TONE
    const float freq_hz = 440.f;
    const int dur_ms = 300;
    const int total = (AUDIO_SAMPLE_RATE_HZ * dur_ms) / 1000;

    for (int pos = 0; pos < total; ) {
        int n = total - pos;
        if (n > TX_MAX_MONO_SAMPLES) {
            n = TX_MAX_MONO_SAMPLES;
        }
        for (int i = 0; i < n; i++) {
            float t = (float)(pos + i) / (float)AUDIO_SAMPLE_RATE_HZ;
            float s = sinf(2.f * (float)M_PI * freq_hz * t);
            int16_t q = (int16_t)(s * 8000.f);
            int32_t v = ((int32_t)q) << 16;
            s_stereo_i32[2 * i] = v;
            s_stereo_i32[2 * i + 1] = v;
        }
        size_t bytes = (size_t)n * 2u * sizeof(int32_t);
        size_t written = 0;
        ESP_ERROR_CHECK(i2s_channel_write(s_tx_handle, s_stereo_i32, bytes, &written, portMAX_DELAY));
        (void)written;
        pos += n;
    }
    ESP_LOGI(TAG, "test tone done (%d ms @ %.0f Hz)", dur_ms, (double)freq_hz);
#endif
}

void tx_i2s_write_mono_pcm16(const int16_t *samples, int n_samples)
{
    if (samples == NULL || n_samples <= 0) {
        return;
    }
    if (n_samples > TX_MAX_MONO_SAMPLES) {
        n_samples = TX_MAX_MONO_SAMPLES;
    }

    for (int i = 0; i < n_samples; i++) {
        int32_t g = ((int32_t)samples[i] * PCM_DIGITAL_GAIN_NUM) / PCM_DIGITAL_GAIN_DEN;
        if (g > 32767) {
            g = 32767;
        }
        if (g < -32768) {
            g = -32768;
        }
        int32_t v = g << 16;
        s_stereo_i32[2 * i] = v;
        s_stereo_i32[2 * i + 1] = v;
    }

    size_t bytes = (size_t)n_samples * 2u * sizeof(int32_t);
    size_t written = 0;
    ESP_ERROR_CHECK(i2s_channel_write(s_tx_handle, s_stereo_i32, bytes, &written, portMAX_DELAY));
}

void tx_i2s_write_silence_mono_frames(int n_mono_samples)
{
    if (n_mono_samples <= 0) {
        return;
    }

    memset(s_stereo_i32, 0, sizeof(s_stereo_i32));

    while (n_mono_samples > 0) {
        int chunk = n_mono_samples;
        if (chunk > TX_MAX_MONO_SAMPLES) {
            chunk = TX_MAX_MONO_SAMPLES;
        }
        size_t bytes = (size_t)chunk * 2u * sizeof(int32_t);
        size_t written = 0;
        ESP_ERROR_CHECK(i2s_channel_write(s_tx_handle, s_stereo_i32, bytes, &written, portMAX_DELAY));
        (void)written;
        n_mono_samples -= chunk;
    }
}
