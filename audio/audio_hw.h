/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2017 Christopher N. Hesse <raymanfx@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SAMSUNG_AUDIO_HW_H
#define SAMSUNG_AUDIO_HW_H

#include <cutils/list.h>
#include <hardware/audio.h>

#include <tinyalsa/asoundlib.h>
#include <tinycompress/tinycompress.h>
/* TODO: remove resampler if possible when AudioFlinger supports downsampling from 48 to 8 */
#include <audio_utils/resampler.h>
#include <audio_route/audio_route.h>
#include "ril_interface.h"

/* Retry for delay in FW loading*/
#define RETRY_NUMBER 10
#define RETRY_US 500000

#define MIXER_CARD 0
#define SOUND_CARD 0

/* Playback */
#define SOUND_DEEP_BUFFER_DEVICE 3
#define SOUND_PLAYBACK_DEVICE 6
#define SOUND_PLAYBACK_SCO_DEVICE 2

/* Capture */
#define SOUND_CAPTURE_DEVICE 0
#define SOUND_CAPTURE_SCO_DEVICE 2

/* Voice calls */
#define SOUND_PLAYBACK_VOICE_DEVICE 1
#define SOUND_CAPTURE_VOICE_DEVICE 1

/* Sound devices specific to the platform
 * The DEVICE_OUT_* and DEVICE_IN_* should be mapped to these sound
 * devices to enable corresponding mixer paths
 */
enum {
    SND_DEVICE_NONE = 0,

    /* Playback devices */
    SND_DEVICE_MIN,
    SND_DEVICE_OUT_BEGIN = SND_DEVICE_MIN,
    SND_DEVICE_OUT_EARPIECE = SND_DEVICE_OUT_BEGIN,
    SND_DEVICE_OUT_SPEAKER,
    SND_DEVICE_OUT_HEADPHONES,
    SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES,
    SND_DEVICE_OUT_BT_SCO,
    SND_DEVICE_OUT_INCALL_EARPIECE,
    SND_DEVICE_OUT_INCALL_EARPIECE_WB,
    SND_DEVICE_OUT_INCALL_SPEAKER,
    SND_DEVICE_OUT_INCALL_SPEAKER_WB,
    SND_DEVICE_OUT_INCALL_HEADPHONES,
    SND_DEVICE_OUT_INCALL_HEADPHONES_WB,
    SND_DEVICE_OUT_INCALL_BT_SCO,
    SND_DEVICE_OUT_INCALL_BT_SCO_WB,
    SND_DEVICE_OUT_COMM_EARPIECE,
    SND_DEVICE_OUT_COMM_SPEAKER,
    SND_DEVICE_OUT_COMM_HEADPHONES,
    SND_DEVICE_OUT_COMM_BT_SCO,
    SND_DEVICE_OUT_END,

    /*
     * Note: IN_BEGIN should be same as OUT_END because total number of devices
     * SND_DEVICES_MAX should not exceed MAX_RX + MAX_TX devices.
     */
    /* Capture devices */
    SND_DEVICE_IN_BEGIN = SND_DEVICE_OUT_END,
    SND_DEVICE_IN_EARPIECE_MIC  = SND_DEVICE_IN_BEGIN,
    SND_DEVICE_IN_SPEAKER_MIC,
    SND_DEVICE_IN_HEADSET_MIC,
    SND_DEVICE_IN_BT_SCO_MIC,
    SND_DEVICE_IN_INCALL_EARPIECE_MIC,
    SND_DEVICE_IN_INCALL_EARPIECE_MIC_WB,
    SND_DEVICE_IN_INCALL_SPEAKER_MIC,
    SND_DEVICE_IN_INCALL_SPEAKER_MIC_WB,
    SND_DEVICE_IN_INCALL_HEADSET_MIC,
    SND_DEVICE_IN_INCALL_HEADSET_MIC_WB,
    SND_DEVICE_IN_INCALL_BT_SCO_MIC,
    SND_DEVICE_IN_INCALL_BT_SCO_MIC_WB,
    SND_DEVICE_IN_COMM_EARPIECE_MIC,
    SND_DEVICE_IN_COMM_SPEAKER_MIC,
    SND_DEVICE_IN_COMM_HEADSET_MIC,
    SND_DEVICE_IN_COMM_BT_SCO_MIC,
    SND_DEVICE_IN_CAMCORDER_MIC,
    SND_DEVICE_IN_CAMCORDER_HEADSET_MIC,
    SND_DEVICE_IN_VOICE_RECOGNITION_MIC,
    SND_DEVICE_IN_VOICE_RECOGNITION_HEADSET_MIC,
    SND_DEVICE_IN_VOICE_RECOGNITION_BT_SCO_MIC,
    SND_DEVICE_IN_END,

    SND_DEVICE_MAX = SND_DEVICE_IN_END,

};


/*
 * tinyAlsa library interprets period size as number of frames
 * one frame = channel_count * sizeof (pcm sample)
 * so if format = 16-bit PCM and channels = Stereo, frame size = 2 ch * 2 = 4 bytes
 * DEEP_BUFFER_OUTPUT_PERIOD_SIZE = 1024 means 1024 * 4 = 4096 bytes
 * We should take care of returning proper size when AudioFlinger queries for
 * the buffer size of an input/output stream
 */
#define PLAYBACK_PERIOD_SIZE 256
#define PLAYBACK_PERIOD_COUNT 2
#define PLAYBACK_DEFAULT_CHANNEL_COUNT 2
#define PLAYBACK_DEFAULT_SAMPLING_RATE 48000
#define PLAYBACK_START_THRESHOLD(size, count) (((size) * (count)) - 1)
#define PLAYBACK_STOP_THRESHOLD(size, count) ((size) * ((count) + 2))
#define PLAYBACK_AVAILABLE_MIN 1


#define SCO_PERIOD_SIZE 168
#define SCO_PERIOD_COUNT 2
#define SCO_DEFAULT_CHANNEL_COUNT 2
#define SCO_DEFAULT_SAMPLING_RATE 8000
#define SCO_WB_SAMPLING_RATE 16000
#define SCO_START_THRESHOLD 335
#define SCO_STOP_THRESHOLD 336
#define SCO_AVAILABLE_MIN 1

#define CAPTURE_PERIOD_SIZE 1024
#define CAPTURE_PERIOD_SIZE_LOW_LATENCY 256
#define CAPTURE_PERIOD_COUNT 2
#define CAPTURE_PERIOD_COUNT_LOW_LATENCY 2
#define CAPTURE_DEFAULT_CHANNEL_COUNT 2
#define CAPTURE_DEFAULT_SAMPLING_RATE 48000
#define CAPTURE_START_THRESHOLD 1

#define DEEP_BUFFER_OUTPUT_SAMPLING_RATE 48000
#define DEEP_BUFFER_OUTPUT_PERIOD_SIZE 480
#define DEEP_BUFFER_OUTPUT_PERIOD_COUNT 8

#define MAX_SUPPORTED_CHANNEL_MASKS 2

typedef int snd_device_t;

/* These are the supported use cases by the hardware.
 * Each usecase is mapped to a specific PCM device.
 * Refer to pcm_device_table[].
 */
typedef enum {
    USECASE_INVALID = -1,
    /* Playback usecases */
    USECASE_AUDIO_PLAYBACK = 0,
    USECASE_AUDIO_PLAYBACK_MULTI_CH,
    USECASE_AUDIO_PLAYBACK_DEEP_BUFFER,

    /* Capture usecases */
    USECASE_AUDIO_CAPTURE,

    USECASE_VOICE_CALL,
    AUDIO_USECASE_MAX
} audio_usecase_t;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/*
 * tinyAlsa library interprets period size as number of frames
 * one frame = channel_count * sizeof (pcm sample)
 * so if format = 16-bit PCM and channels = Stereo, frame size = 2 ch * 2 = 4 bytes
 * DEEP_BUFFER_OUTPUT_PERIOD_SIZE = 1024 means 1024 * 4 = 4096 bytes
 * We should take care of returning proper size when AudioFlinger queries for
 * the buffer size of an input/output stream
 */

typedef enum {
    PCM_PLAYBACK,
    PCM_CAPTURE,
    VOICE_CALL,
} usecase_type_t;

struct pcm_device_profile {
    struct pcm_config config;
    int               card;
    int               id;
    usecase_type_t    type;
    audio_devices_t   devices;
};

struct pcm_device {
    struct listnode            stream_list_node;
    struct pcm_device_profile* pcm_profile;
    struct pcm*                pcm;
    int                        status;
};

struct stream_out {
    struct audio_stream_out     stream;
    pthread_mutex_t             lock; /* see note below on mutex acquisition order */
    pthread_mutex_t             pre_lock; /* acquire before lock to avoid DOS by playback thread */
    pthread_cond_t              cond;
    struct pcm_config           config;
    struct listnode             pcm_dev_list;
    int                         standby;
    unsigned int                sample_rate;
    audio_channel_mask_t        channel_mask;
    audio_format_t              format;
    audio_devices_t             devices;
    audio_output_flags_t        flags;
    audio_usecase_t             usecase;
    /* Array of supported channel mask configurations. +1 so that the last entry is always 0 */
    audio_channel_mask_t        supported_channel_masks[MAX_SUPPORTED_CHANNEL_MASKS + 1];
    bool                        muted;
    /* total frames written, not cleared when entering standby */
    uint64_t                    written;
    audio_io_handle_t           handle;
    int                         non_blocking;
    int                         send_new_metadata;
    struct audio_device*        dev;
    void *proc_buf_out;
    size_t proc_buf_size;
    int64_t                     last_write_time_us;
};

struct stream_in {
    struct audio_stream_in              stream;
    pthread_mutex_t                     lock; /* see note below on mutex acquisition order */
    pthread_mutex_t                     pre_lock; /* acquire before lock to avoid DOS by
                                                     capture thread */
    struct pcm_config                   config;
    struct listnode                     pcm_dev_list;
    int                                 standby;
    audio_source_t                      source;
    audio_devices_t                     devices;
    uint32_t                            main_channels;
    audio_usecase_t                     usecase;
    usecase_type_t                      usecase_type;
    bool                                enable_aec;
    bool                                enable_ns;
    audio_input_flags_t                 input_flags;

    /* TODO: remove resampler if possible when AudioFlinger supports downsampling from 48 to 8 */
    unsigned int                        requested_rate;
    struct resampler_itfe*              resampler;
    struct resampler_buffer_provider    buf_provider;
    int                                 read_status;
    int16_t*                            read_buf;
    size_t                              read_buf_size;
    size_t                              read_buf_frames;

    void *proc_buf_out;
    size_t proc_buf_size;

    struct audio_device*                dev;

    int64_t                             last_read_time_us;
    int64_t                             frames_read; /* total frames read, not cleared when
                                                        entering standby */
};

struct mixer_card {
    struct listnode     adev_list_node;
    struct listnode     uc_list_node[AUDIO_USECASE_MAX];
    int                 card;
    struct mixer*       mixer;
    struct audio_route* audio_route;
    struct timespec     dsp_poweroff_time;
};

struct audio_usecase {
    struct listnode         adev_list_node;
    audio_usecase_t         id;
    usecase_type_t          type;
    audio_devices_t         devices;
    snd_device_t            out_snd_device;
    snd_device_t            in_snd_device;
    struct audio_stream*    stream;
    struct listnode         mixer_list;
};

struct audio_device {
    struct audio_hw_device  device;
    pthread_mutex_t         lock; /* see note below on mutex acquisition order */
    struct listnode         mixer_list;
    audio_mode_t            mode;
    struct stream_in*       active_input;
    struct stream_out*      primary_output;
    bool                    mic_mute;
    bool                    screen_off;
    bool                    in_call;
    bool                    in_call_sco;
    bool                    in_comm_sco;
    float                   volume;
    bool                    bluetooth_nrec;
    bool                    bluetooth_wb;
    bool                    audience_enabled;
    bool                    forced_voice_config;
    int                     wb_amr_type;
    struct ril_handle       ril;

    struct pcm *pcm_voice_rx;
    struct pcm *pcm_voice_tx;
    struct pcm *pcm_sco_rx;
    struct pcm *pcm_sco_tx;

    int*                    snd_dev_ref_cnt;
    struct listnode         usecase_list;
    bool                    ns_in_voice_rec;
    pthread_mutex_t         lock_inputs; /* see note below on mutex acquisition order */
};

/*
 * NOTE: when multiple mutexes have to be acquired, always take the
 * lock_inputs, stream_in, stream_out, then audio_device mutex.
 * stream_in mutex must always be before stream_out mutex
 * lock_inputs must be held in order to either close the input stream, or prevent closure.
 */

#endif // SAMSUNG_AUDIO_HW_H
