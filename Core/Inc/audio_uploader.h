#ifndef AUDIO_UPLOADER_H
#define AUDIO_UPLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "four_g_mqtt.h"
#include <stdbool.h>
#include <stdint.h>

#define AUDIO_UPLOADER_FILENAME_MAX  24U

typedef enum {
    AUDIO_UPLOADER_OK = 0,
    AUDIO_UPLOADER_ERROR,
    AUDIO_UPLOADER_INVALID_PARAM,
    AUDIO_UPLOADER_QUEUE_FULL,
    AUDIO_UPLOADER_FILE_ERROR,
    AUDIO_UPLOADER_ENCODE_ERROR,
    AUDIO_UPLOADER_MQTT_ERROR
} AudioUploaderResult_t;

void AudioUploader_Init(void);
AudioUploaderResult_t AudioUploader_EnqueueFile(const char *filename, uint32_t session_id, bool final_file);
void AudioUploader_MarkLastQueuedFileFinal(uint32_t session_id);
void AudioUploader_Process(void);
void AudioUploader_Abort(void);
bool AudioUploader_IsBusy(void);
AudioUploaderResult_t AudioUploader_GetLastResult(void);
FourGMqttResult_t AudioUploader_GetLastMqttResult(void);
uint32_t AudioUploader_GetUploadedFiles(void);
uint32_t AudioUploader_GetUploadedChunks(void);
uint32_t AudioUploader_GetDroppedFiles(void);
uint8_t AudioUploader_GetQueueCount(void);

#ifdef __cplusplus
}
#endif

#endif
