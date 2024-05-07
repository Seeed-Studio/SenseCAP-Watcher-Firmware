/*
 * SPDX-FileCopyrightText: 2024 Seeed Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"

#include "sensecap-watcher.h"

#include "console_audio.h"

#define BUFFER_SIZE      (1024)
#define SAMPLE_RATE      (16000) // For recording
#define SAMPLE_CHANNELS  (1)
#define DEFAULT_VOLUME   (100)
#define RECORDING_LENGTH (300)
typedef struct __attribute__((packed))
{
    uint8_t ignore_0[22];
    uint16_t num_channels;
    uint32_t sample_rate;
    uint8_t ignore_1[6];
    uint16_t bits_per_sample;
    uint8_t ignore_2[4];
    uint32_t data_size;
    uint8_t data[];
} dumb_wav_header_t;

/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = { .name = "console_cmd_audio", .plugin_regd_fn = &console_cmd_audio_register };

typedef struct audio_op_t
{
    char *name;
    esp_err_t (*operation)(struct audio_op_t *self, int argc, char *argv[]);
    int arg_cnt;
    int start_index;
    char *help;
} audio_op_t;

static esp_err_t audio_help_op(audio_op_t *self, int argc, char *argv[]);
static esp_err_t audio_play_op(audio_op_t *self, int argc, char *argv[]);
static esp_err_t audio_record_op(audio_op_t *self, int argc, char *argv[]);
static esp_err_t audio_volume_op(audio_op_t *self, int argc, char *argv[]);
static esp_err_t audio_mute_op(audio_op_t *self, int argc, char *argv[]);

static const char *TAG = "console_audio";

typedef struct
{
    char *filename;
} playback_t;

typedef struct
{
    char *filename;
    uint32_t seconds;
} recording_t;

static QueueHandle_t audio_play_q;
static QueueHandle_t audio_record_q;

static audio_op_t cmd_list[] = {
    { .name = "help", .operation = audio_help_op, .arg_cnt = 2, .start_index = 1, .help = "audio help: Prints the help text for all audio commands" },
    { .name = "play", .operation = audio_play_op, .arg_cnt = 3, .start_index = 1, .help = "audio play <file path>: Play audio" },
    { .name = "record", .operation = audio_record_op, .arg_cnt = 4, .start_index = 1, .help = "audio record <file path> <seconds>: Record audio" },
    { .name = "volume", .operation = audio_volume_op, .arg_cnt = 3, .start_index = 1, .help = "audio volume <value>: Set volume" },
    { .name = "mute", .operation = audio_mute_op, .arg_cnt = 3, .start_index = 1, .help = "audio mute: Mute audio" },
};

static void audio_play_task(void *arg)
{
    int16_t *wav_bytes = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DEFAULT);
    while (1)
    {
        playback_t playback;
        if (xQueueReceive(audio_play_q, &playback, portMAX_DELAY) == pdTRUE)
        {
            FILE *play_file = fopen(playback.filename, "rb");
            if (play_file == NULL)
            {
                ESP_LOGE(TAG, "Open file error: %s", playback.filename);
                free(playback.filename);
                continue;
            }
            dumb_wav_header_t wav_header;
            if (fread((void *)&wav_header, 1, sizeof(wav_header), play_file) != sizeof(wav_header))
            {
                ESP_LOGW(TAG, "Error in reading file");
                free(playback.filename);
                continue;
            }
            ESP_LOGI(TAG, "\nPlaying %s ...", playback.filename);
            bsp_codec_set_fs(wav_header.sample_rate, wav_header.bits_per_sample, wav_header.num_channels);
            size_t bytes_send_to_i2s = 0;
            while (bytes_send_to_i2s < wav_header.data_size)
            {
                size_t bytes_read = fread(wav_bytes, 1, BUFFER_SIZE, play_file);
                ESP_ERROR_CHECK(bsp_i2s_write(wav_bytes, bytes_read, &bytes_read, 0));
                bytes_send_to_i2s += bytes_read;
            }
            bsp_codec_dev_stop();
            ESP_LOGI(TAG, "Playing stop, length: %i bytes", bytes_send_to_i2s);
            fclose(play_file);
            free(playback.filename);
        }
    }
    free(wav_bytes);
}

static void audio_record_task(void *arg)
{
    int16_t *wav_bytes = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DEFAULT);
    assert(wav_bytes != NULL);
    while (1)
    {
        recording_t recording;
        if (xQueueReceive(audio_record_q, &recording, portMAX_DELAY) == pdTRUE)
        {
            FILE *record_file = fopen(recording.filename, "wb");
            if (record_file == NULL)
            {
                ESP_LOGE(TAG, "Open file error: %s", recording.filename);
                free(recording.filename);
                continue;
            }

            dumb_wav_header_t wav_header = { .bits_per_sample = 16, .data_size = RECORDING_LENGTH * BUFFER_SIZE, .num_channels = SAMPLE_CHANNELS, .sample_rate = SAMPLE_RATE };
            uint32_t data_size = sizeof(dumb_wav_header_t) + wav_header.data_size - 4;
            memcpy(&wav_header.ignore_0[0], "RIFF", 4);
            memcpy(&wav_header.ignore_0[4], &data_size, 4);
            memcpy(&wav_header.ignore_0[8], "WAVEfmt ", 8);

            if (fwrite((void *)&wav_header, 1, sizeof(dumb_wav_header_t), record_file) != sizeof(dumb_wav_header_t))
            {
                ESP_LOGW(TAG, "Error in writing to file");
                free(recording.filename);
                continue;
            }

            setvbuf(record_file, NULL, _IOFBF, BUFFER_SIZE);

            ESP_LOGI(TAG, "\nRecording start %d seconds... \n", recording.seconds);

            bsp_codec_set_fs(SAMPLE_RATE, 16, 2);
            size_t bytes_written = 0;
            size_t bytes_write = recording.seconds * SAMPLE_RATE * SAMPLE_CHANNELS * 2;
            while (bytes_written < bytes_write)
            {
                size_t data_written = 0;
                ESP_ERROR_CHECK(bsp_i2s_read(wav_bytes, BUFFER_SIZE, &data_written, 0));
                bytes_written += fwrite(wav_bytes, 1, BUFFER_SIZE, record_file);
            }
            bsp_codec_dev_stop();
            ESP_LOGI(TAG, "Recording stop, length: %i bytes", bytes_written);
            fclose(record_file);
            free(recording.filename);
        }
    }
}

static esp_err_t audio_help_op(audio_op_t *self, int argc, char *argv[])
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);

    for (int i = 0; i < cmd_count; i++)
    {
        if ((cmd_list[i].help != NULL) && (strlen(cmd_list[i].help) != 0))
        {
            printf(" %s\n", cmd_list[i].help);
        }
    }

    return ESP_OK;
}

static esp_err_t audio_play_op(audio_op_t *self, int argc, char *argv[])
{
    playback_t playback;

    playback.filename = strdup(argv[2]);

    if (xQueueSend(audio_play_q, &playback, pdMS_TO_TICKS(100)) != pdPASS)
    {
        ESP_LOGE(TAG, "Busy...");
        return ESP_OK;
    }

    return ESP_OK;
}

static esp_err_t audio_record_op(audio_op_t *self, int argc, char *argv[])
{
    recording_t recording;

    recording.filename = strdup(argv[2]);
    recording.seconds = atoi(argv[3]);

    if (xQueueSend(audio_record_q, &recording, pdMS_TO_TICKS(100)) != pdPASS)
    {
        ESP_LOGE(TAG, "Busy...");
        return ESP_OK;
    }

    return ESP_OK;
}

static esp_err_t audio_volume_op(audio_op_t *self, int argc, char *argv[])
{
    int volume = atoi(argv[2]);
    bsp_codec_volume_set(volume, NULL);
    return ESP_OK;
}

static esp_err_t audio_mute_op(audio_op_t *self, int argc, char *argv[])
{
    int mute = atoi(argv[2]);
    bsp_codec_mute_set(mute == 0 ? false : true);
    return ESP_OK;
}

/* handle 'audio' command */
static esp_err_t do_cmd_audio(int argc, char **argv)
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);
    audio_op_t cmd;

    for (int i = 0; i < cmd_count; i++)
    {
        cmd = cmd_list[i];

        if (argc < cmd.start_index + 1)
        {
            continue;
        }

        if (!strcmp(cmd.name, argv[cmd.start_index]))
        {
            /* Get interface for eligible commands */
            if (cmd.arg_cnt == argc)
            {
                if (cmd.operation != NULL)
                {
                    if (cmd.operation(&cmd, argc, argv) != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Usage:\n%s", cmd.help);
                        return 0;
                    }
                }
                return ESP_OK;
            }
        }
    }

    ESP_LOGE(TAG, "Command not available");

    return ESP_OK;
}

/**
 * @brief Registers the audio command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_audio_register(void)
{
    esp_err_t ret;

    bsp_io_expander_init();

    bsp_spiffs_init_default();
    bsp_sdcard_init_default();

    bsp_codec_init();
    bsp_codec_volume_set(DEFAULT_VOLUME, NULL);

    audio_play_q = xQueueCreate(1, sizeof(playback_t));
    assert(audio_play_q != NULL);

    audio_record_q = xQueueCreate(1, sizeof(recording_t));
    assert(audio_record_q != NULL);

    assert(xTaskCreate(audio_play_task, "audio_play_task", 4096, NULL, 6, NULL) == pdPASS);
    assert(xTaskCreate(audio_record_task, "audio_record_task", 4096, NULL, 6, NULL) == pdPASS);

    esp_console_cmd_t command = { .command = "audio", .help = "Command for audio operations\n For more info run 'audio help'", .func = &do_cmd_audio };

    ret = esp_console_cmd_register(&command);
    if (ret)
    {
        ESP_LOGE(TAG, "Unable to register audio");
    }

    return ret;
}
