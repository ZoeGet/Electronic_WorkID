#include "audio_uploader.h"

#include "base64_codec.h"
#include "fatfs.h"
#include "wav_format.h"
#include <stdio.h>
#include <string.h>

#define AUDIO_UPLOAD_TOPIC              "asr/device/CARD0001/audio/chunk"
#define AUDIO_UPLOAD_QUEUE_DEPTH        8U
#define AUDIO_UPLOAD_PCM_CHUNK_BYTES    128U         //PCM每段大小
#define AUDIO_UPLOAD_B64_BUFFER_SIZE    173U         //Base64编码后每段大小
#define AUDIO_UPLOAD_PAYLOAD_SIZE       420U         //Payload大小
#define AUDIO_UPLOAD_MIN_INTERVAL_MS    1000U        //最小间隔时间，单位ms
#define AUDIO_UPLOAD_FALLBACK_TIME_MS   1779195600000ULL       //默认时间戳
#define AUDIO_UPLOAD_RETRY_DELAY_MS     10000U       //重试间隔时间，单位ms

typedef struct {
    char filename[AUDIO_UPLOADER_FILENAME_MAX];
    uint32_t session_id;
    bool final_file;
} AudioUploadFileItem_t;

typedef enum {
    AUDIO_UPLOAD_STATE_IDLE = 0,
    AUDIO_UPLOAD_STATE_CONNECT_WAIT,
    AUDIO_UPLOAD_STATE_PUBLISH_WAIT,
    AUDIO_UPLOAD_STATE_DISCONNECT_WAIT
} AudioUploadState_t;

typedef struct {
    bool file_open;
    bool active_item_valid;
    AudioUploadFileItem_t queue[AUDIO_UPLOAD_QUEUE_DEPTH];
    AudioUploadFileItem_t active_item;
    uint8_t read_index;
    uint8_t write_index;
    uint8_t queue_count;
    FIL file;
    uint8_t pcm[AUDIO_UPLOAD_PCM_CHUNK_BYTES];
    char base64[AUDIO_UPLOAD_B64_BUFFER_SIZE];
    char payload[AUDIO_UPLOAD_PAYLOAD_SIZE];
    uint32_t uploaded_files;
    uint32_t uploaded_chunks;
    uint32_t dropped_files;
    uint32_t last_process_tick;
    uint32_t mqtt_retry_tick;
    uint32_t connected_session_id;
    uint32_t session_timestamp_id;
    uint64_t session_timestamp_ms;
    uint32_t session_sequence;
    AudioUploadState_t state;
    FSIZE_t pending_chunk_start;
    uint16_t pending_pcm_length;
    bool pending_final_chunk;
    AudioUploaderResult_t last_result;
    FourGMqttResult_t last_mqtt_result;
} AudioUploaderContext_t;

static AudioUploaderContext_t gAudioUploader;

static bool AudioUploader_QueueIsFull(void)
{
    return gAudioUploader.queue_count >= AUDIO_UPLOAD_QUEUE_DEPTH;
}

static bool AudioUploader_QueueIsEmpty(void)
{
    return gAudioUploader.queue_count == 0U;
}

static void AudioUploader_PopQueue(AudioUploadFileItem_t *item)
{
    if ((item == NULL) || AudioUploader_QueueIsEmpty()) {
        return;
    }

    *item = gAudioUploader.queue[gAudioUploader.read_index];
    gAudioUploader.read_index = (uint8_t)((gAudioUploader.read_index + 1U) % AUDIO_UPLOAD_QUEUE_DEPTH);
    gAudioUploader.queue_count--;
}

static AudioUploaderResult_t AudioUploader_OpenActiveFile(void)
{
    FRESULT fr;

    fr = f_open(&gAudioUploader.file, gAudioUploader.active_item.filename, FA_READ);
    if (fr != FR_OK) {
        gAudioUploader.dropped_files++;
        gAudioUploader.last_result = AUDIO_UPLOADER_FILE_ERROR;
        return gAudioUploader.last_result;
    }

    fr = f_lseek(&gAudioUploader.file, WAV_HEADER_SIZE);
    if (fr != FR_OK) {
        (void)f_close(&gAudioUploader.file);
        gAudioUploader.dropped_files++;
        gAudioUploader.last_result = AUDIO_UPLOADER_FILE_ERROR;
        return gAudioUploader.last_result;
    }

    gAudioUploader.file_open = true;
    gAudioUploader.last_result = AUDIO_UPLOADER_OK;
    return gAudioUploader.last_result;
}

static AudioUploaderResult_t AudioUploader_LoadNextFile(void)
{
    while (!AudioUploader_QueueIsEmpty()) {
        AudioUploader_PopQueue(&gAudioUploader.active_item);
        gAudioUploader.active_item_valid = true;
        if (AudioUploader_OpenActiveFile() == AUDIO_UPLOADER_OK) {
            return AUDIO_UPLOADER_OK;
        }
        gAudioUploader.active_item_valid = false;
    }

    return AUDIO_UPLOADER_OK;
}

static bool AudioUploader_AppendText(char *buffer, uint16_t buffer_size, uint16_t *offset, const char *text)
{
    uint16_t text_len;

    if ((buffer == NULL) || (offset == NULL) || (text == NULL)) {
        return false;
    }

    text_len = (uint16_t)strlen(text);
    if (((uint32_t)(*offset) + text_len) >= buffer_size) {
        return false;
    }

    memcpy(&buffer[*offset], text, text_len);
    *offset = (uint16_t)(*offset + text_len);
    buffer[*offset] = '\0';
    return true;
}

static bool AudioUploader_AppendUInt32(char *buffer, uint16_t buffer_size, uint16_t *offset, uint32_t value)
{
    char digits[10];
    uint8_t count = 0U;

    do {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(digits)));

    while (count > 0U) {
        char ch[2];
        ch[0] = digits[--count];
        ch[1] = '\0';
        if (!AudioUploader_AppendText(buffer, buffer_size, offset, ch)) {
            return false;
        }
    }

    return true;
}

static bool AudioUploader_AppendUInt64(char *buffer, uint16_t buffer_size, uint16_t *offset, uint64_t value)
{
    char digits[20];
    uint8_t count = 0U;

    do {
        digits[count++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    } while ((value != 0ULL) && (count < sizeof(digits)));

    while (count > 0U) {
        char ch[2];
        ch[0] = digits[--count];
        ch[1] = '\0';
        if (!AudioUploader_AppendText(buffer, buffer_size, offset, ch)) {
            return false;
        }
    }

    return true;
}

static AudioUploaderResult_t AudioUploader_PrepareChunkPayload(const uint8_t *pcm, uint16_t pcm_length, bool is_final)
{
    uint16_t encoded_length;
    uint16_t offset = 0U;

    if ((pcm == NULL) || (pcm_length == 0U)) {
        gAudioUploader.last_result = AUDIO_UPLOADER_INVALID_PARAM;
        return gAudioUploader.last_result;
    }

    encoded_length = Base64_Encode(pcm, pcm_length, gAudioUploader.base64, sizeof(gAudioUploader.base64));
    if (encoded_length == 0U) {
        gAudioUploader.last_result = AUDIO_UPLOADER_ENCODE_ERROR;
        return gAudioUploader.last_result;
    }

    gAudioUploader.payload[0] = '\0';
    if (!AudioUploader_AppendText(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, "{\"session_id\":\"") ||
        !AudioUploader_AppendUInt32(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, gAudioUploader.active_item.session_id) ||
        !AudioUploader_AppendText(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, "\",\"sequence\":") ||
        !AudioUploader_AppendUInt32(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, gAudioUploader.session_sequence) ||
        !AudioUploader_AppendText(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, ",\"timestamp\":") ||
        !AudioUploader_AppendUInt64(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, gAudioUploader.session_timestamp_ms) ||
        !AudioUploader_AppendText(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, ",\"is_final\":") ||
        !AudioUploader_AppendText(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, is_final ? "true" : "false") ||
        !AudioUploader_AppendText(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, ",\"audio_base64\":\"") ||
        !AudioUploader_AppendText(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, gAudioUploader.base64) ||
        !AudioUploader_AppendText(gAudioUploader.payload, sizeof(gAudioUploader.payload), &offset, "\",\"format\":\"pcm\"}")) {
        gAudioUploader.last_result = AUDIO_UPLOADER_ENCODE_ERROR;
        return gAudioUploader.last_result;
    }

    gAudioUploader.last_result = AUDIO_UPLOADER_OK;
    return gAudioUploader.last_result;
}

void AudioUploader_Init(void)
{
    memset(&gAudioUploader, 0, sizeof(gAudioUploader));
    gAudioUploader.last_result = AUDIO_UPLOADER_OK;
    gAudioUploader.last_mqtt_result = FOUR_G_MQTT_OK;
}

void AudioUploader_Abort(void)
{
    if (gAudioUploader.file_open) {
        (void)f_close(&gAudioUploader.file);
    }

    gAudioUploader.file_open = false;
    gAudioUploader.active_item_valid = false;
    gAudioUploader.read_index = 0U;
    gAudioUploader.write_index = 0U;
    gAudioUploader.queue_count = 0U;
    gAudioUploader.connected_session_id = 0U;
    gAudioUploader.state = AUDIO_UPLOAD_STATE_IDLE;
    gAudioUploader.mqtt_retry_tick = 0U;
    gAudioUploader.pending_chunk_start = 0U;
    gAudioUploader.pending_pcm_length = 0U;
    gAudioUploader.pending_final_chunk = false;
    gAudioUploader.last_result = AUDIO_UPLOADER_OK;
    gAudioUploader.last_mqtt_result = FOUR_G_MQTT_OK;
}

AudioUploaderResult_t AudioUploader_EnqueueFile(const char *filename, uint32_t session_id, bool final_file)
{
    AudioUploadFileItem_t *item;

    if ((filename == NULL) || (filename[0] == '\0') || (session_id == 0U)) {
        gAudioUploader.last_result = AUDIO_UPLOADER_INVALID_PARAM;
        return gAudioUploader.last_result;
    }

    if (AudioUploader_QueueIsFull()) {
        gAudioUploader.dropped_files++;
        gAudioUploader.last_result = AUDIO_UPLOADER_QUEUE_FULL;
        return gAudioUploader.last_result;
    }

    item = &gAudioUploader.queue[gAudioUploader.write_index];
    memset(item, 0, sizeof(*item));
    (void)snprintf(item->filename, sizeof(item->filename), "%s", filename);
    item->session_id = session_id;
    item->final_file = final_file;

    gAudioUploader.write_index = (uint8_t)((gAudioUploader.write_index + 1U) % AUDIO_UPLOAD_QUEUE_DEPTH);
    gAudioUploader.queue_count++;
    gAudioUploader.last_result = AUDIO_UPLOADER_OK;
    return gAudioUploader.last_result;
}

void AudioUploader_MarkLastQueuedFileFinal(uint32_t session_id)
{
    if (gAudioUploader.file_open &&
        gAudioUploader.active_item_valid &&
        (gAudioUploader.active_item.session_id == session_id) &&
        AudioUploader_QueueIsEmpty()) {
        gAudioUploader.active_item.final_file = true;
        return;
    }

    for (uint8_t i = 0U; i < gAudioUploader.queue_count; i++) {
        uint8_t index = (uint8_t)((gAudioUploader.read_index + i) % AUDIO_UPLOAD_QUEUE_DEPTH);
        if (gAudioUploader.queue[index].session_id == session_id) {
            gAudioUploader.queue[index].final_file = false;
        }
    }

    if (gAudioUploader.queue_count > 0U) {
        uint8_t last_index = (uint8_t)((gAudioUploader.write_index + AUDIO_UPLOAD_QUEUE_DEPTH - 1U) % AUDIO_UPLOAD_QUEUE_DEPTH);
        if (gAudioUploader.queue[last_index].session_id == session_id) {
            gAudioUploader.queue[last_index].final_file = true;
        }
    }
}

void AudioUploader_Process(void)
{
    UINT bytes_read = 0;
    FRESULT fr;
    bool is_eof;
    bool is_final_chunk;
    uint32_t now = HAL_GetTick();
    FourGMqttAsyncStatus_t async_status;

    if ((gAudioUploader.mqtt_retry_tick != 0U) &&
        ((now - gAudioUploader.mqtt_retry_tick) < AUDIO_UPLOAD_RETRY_DELAY_MS)) {
        return;
    }
    gAudioUploader.mqtt_retry_tick = 0U;

    switch (gAudioUploader.state) {
        case AUDIO_UPLOAD_STATE_CONNECT_WAIT:
            async_status = FourG_MQTT_AsyncProcess();
            if (async_status == FOUR_G_MQTT_ASYNC_BUSY) {
                return;
            }
            if (async_status != FOUR_G_MQTT_ASYNC_DONE) {
                if (gAudioUploader.file_open) {
                    (void)f_close(&gAudioUploader.file);
                    gAudioUploader.file_open = false;
                }
                gAudioUploader.active_item_valid = false;
                gAudioUploader.dropped_files++;
                gAudioUploader.last_mqtt_result = FourG_MQTT_AsyncGetResult();
                gAudioUploader.last_result = AUDIO_UPLOADER_MQTT_ERROR;
                gAudioUploader.connected_session_id = 0U;
                gAudioUploader.state = AUDIO_UPLOAD_STATE_IDLE;
                gAudioUploader.mqtt_retry_tick = 0U;
                return;
            }

            gAudioUploader.last_mqtt_result = FOUR_G_MQTT_OK;
            gAudioUploader.connected_session_id = gAudioUploader.active_item.session_id;
            if (gAudioUploader.session_timestamp_id != gAudioUploader.active_item.session_id) {
                gAudioUploader.session_timestamp_ms = AUDIO_UPLOAD_FALLBACK_TIME_MS;
                gAudioUploader.session_timestamp_id = gAudioUploader.active_item.session_id;
                gAudioUploader.session_sequence = 0U;
            }
            gAudioUploader.state = AUDIO_UPLOAD_STATE_IDLE;
            return;

        case AUDIO_UPLOAD_STATE_PUBLISH_WAIT:
            async_status = FourG_MQTT_AsyncProcess();
            if (async_status == FOUR_G_MQTT_ASYNC_BUSY) {
                return;
            }
            if (async_status != FOUR_G_MQTT_ASYNC_DONE) {
                if (gAudioUploader.file_open) {
                    (void)f_close(&gAudioUploader.file);
                    gAudioUploader.file_open = false;
                }
                gAudioUploader.active_item_valid = false;
                gAudioUploader.dropped_files++;
                gAudioUploader.last_mqtt_result = FourG_MQTT_AsyncGetResult();
                gAudioUploader.last_result = AUDIO_UPLOADER_MQTT_ERROR;
                gAudioUploader.connected_session_id = 0U;
                gAudioUploader.state = AUDIO_UPLOAD_STATE_IDLE;
                gAudioUploader.mqtt_retry_tick = 0U;
                return;
            }

            gAudioUploader.uploaded_chunks++;
            gAudioUploader.session_sequence++;
            gAudioUploader.last_mqtt_result = FOUR_G_MQTT_OK;
            gAudioUploader.last_result = AUDIO_UPLOADER_OK;
            if (gAudioUploader.pending_final_chunk) {
                (void)f_close(&gAudioUploader.file);
                gAudioUploader.file_open = false;
                gAudioUploader.active_item_valid = false;
                gAudioUploader.uploaded_files++;
            }
            gAudioUploader.state = AUDIO_UPLOAD_STATE_IDLE;
            return;

        case AUDIO_UPLOAD_STATE_DISCONNECT_WAIT:
            async_status = FourG_MQTT_AsyncProcess();
            if (async_status == FOUR_G_MQTT_ASYNC_BUSY) {
                return;
            }
            gAudioUploader.connected_session_id = 0U;
            gAudioUploader.state = AUDIO_UPLOAD_STATE_IDLE;
            gAudioUploader.mqtt_retry_tick = 0U;
            return;

        case AUDIO_UPLOAD_STATE_IDLE:
        default:
            break;
    }

    if ((gAudioUploader.last_process_tick != 0U) &&
        ((now - gAudioUploader.last_process_tick) < AUDIO_UPLOAD_MIN_INTERVAL_MS)) {
        return;
    }
    gAudioUploader.last_process_tick = now;

    if (!gAudioUploader.file_open) {
        (void)AudioUploader_LoadNextFile();
        if (!gAudioUploader.file_open) {
            return;
        }
    }

    if (gAudioUploader.connected_session_id != gAudioUploader.active_item.session_id) {
        if (FourG_MQTT_AsyncConnectStart() == FOUR_G_MQTT_OK) {
            gAudioUploader.state = AUDIO_UPLOAD_STATE_CONNECT_WAIT;
        } else {
            if (gAudioUploader.file_open) {
                (void)f_close(&gAudioUploader.file);
                gAudioUploader.file_open = false;
            }
            gAudioUploader.active_item_valid = false;
            gAudioUploader.dropped_files++;
            gAudioUploader.last_result = AUDIO_UPLOADER_MQTT_ERROR;
            gAudioUploader.mqtt_retry_tick = 0U;
        }
        return;
    }

    gAudioUploader.pending_chunk_start = f_tell(&gAudioUploader.file);
    fr = f_read(&gAudioUploader.file, gAudioUploader.pcm, AUDIO_UPLOAD_PCM_CHUNK_BYTES, &bytes_read);
    if (fr != FR_OK) {
        (void)f_close(&gAudioUploader.file);
        gAudioUploader.file_open = false;
        gAudioUploader.active_item_valid = false;
        gAudioUploader.dropped_files++;
        gAudioUploader.last_result = AUDIO_UPLOADER_FILE_ERROR;
        return;
    }

    if (bytes_read == 0U) {
        (void)f_close(&gAudioUploader.file);
        gAudioUploader.file_open = false;
        gAudioUploader.active_item_valid = false;
        gAudioUploader.uploaded_files++;
        gAudioUploader.last_result = AUDIO_UPLOADER_OK;
        return;
    }

    is_eof = f_eof(&gAudioUploader.file) != 0;
    is_final_chunk = gAudioUploader.active_item.final_file && is_eof;
    if (AudioUploader_PrepareChunkPayload(gAudioUploader.pcm, (uint16_t)bytes_read, is_final_chunk) != AUDIO_UPLOADER_OK) {
        return;
    }

    gAudioUploader.pending_pcm_length = (uint16_t)bytes_read;
    gAudioUploader.pending_final_chunk = is_final_chunk;
    if (FourG_MQTT_AsyncPublishRawStart(AUDIO_UPLOAD_TOPIC, gAudioUploader.payload, (uint16_t)strlen(gAudioUploader.payload)) != FOUR_G_MQTT_OK) {
        if (gAudioUploader.file_open) {
            (void)f_close(&gAudioUploader.file);
            gAudioUploader.file_open = false;
        }
        gAudioUploader.active_item_valid = false;
        gAudioUploader.dropped_files++;
        gAudioUploader.last_result = AUDIO_UPLOADER_MQTT_ERROR;
        gAudioUploader.connected_session_id = 0U;
        gAudioUploader.mqtt_retry_tick = 0U;
        return;
    }

    gAudioUploader.state = AUDIO_UPLOAD_STATE_PUBLISH_WAIT;
}

bool AudioUploader_IsBusy(void)
{
    return (gAudioUploader.state != AUDIO_UPLOAD_STATE_IDLE) ||
           gAudioUploader.file_open ||
           !AudioUploader_QueueIsEmpty();
}

AudioUploaderResult_t AudioUploader_GetLastResult(void)
{
    return gAudioUploader.last_result;
}

FourGMqttResult_t AudioUploader_GetLastMqttResult(void)
{
    return gAudioUploader.last_mqtt_result;
}

uint32_t AudioUploader_GetUploadedFiles(void)
{
    return gAudioUploader.uploaded_files;
}

uint32_t AudioUploader_GetUploadedChunks(void)
{
    return gAudioUploader.uploaded_chunks;
}

uint32_t AudioUploader_GetDroppedFiles(void)
{
    return gAudioUploader.dropped_files;
}

uint8_t AudioUploader_GetQueueCount(void)
{
    return gAudioUploader.queue_count;
}
