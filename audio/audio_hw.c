/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2017 Christopher N. Hesse <raymanfx@gmail.com>
 * Copyright (C) 2017 Andreas Schneider <asn@cryptomilk.org>
 * Copyright (C) 2018 The LineageOS Project
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

#define LOG_TAG "audio_hw_primary"
#define LOG_NDEBUG 0
/*#define VERY_VERY_VERBOSE_LOGGING*/
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>
#include <dlfcn.h>
#include <fcntl.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/atomic.h>
#include <cutils/sched_policy.h>
#include <cutils/properties.h>

#include <hardware/audio_effect.h>
#include <system/thread_defs.h>
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_ns.h>
#include <audio_utils/channels.h>
#include "audio_hw.h"
#include "audience.h"

static struct pcm_config pcm_config_voicecall = {
    .channels = 2,
    .rate = 16000,
    .period_size = CAPTURE_PERIOD_SIZE_LOW_LATENCY,
    .period_count = CAPTURE_PERIOD_COUNT_LOW_LATENCY,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_voice_sco = {
    .channels = 1,
    .rate = SCO_DEFAULT_SAMPLING_RATE,
    .period_size = SCO_PERIOD_SIZE,
    .period_count = SCO_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_voice_sco_wb = {
    .channels = 1,
    .rate = SCO_WB_SAMPLING_RATE,
    .period_size = SCO_PERIOD_SIZE,
    .period_count = SCO_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

/* TODO: the following PCM device profiles could be read from a config file */
static struct pcm_device_profile pcm_device_playback = {
    .config = {
        .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
        .rate = PLAYBACK_DEFAULT_SAMPLING_RATE,
        .period_size = PLAYBACK_PERIOD_SIZE,
        .period_count = PLAYBACK_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = PLAYBACK_START_THRESHOLD(PLAYBACK_PERIOD_SIZE, PLAYBACK_PERIOD_COUNT),
        .stop_threshold = PLAYBACK_STOP_THRESHOLD(PLAYBACK_PERIOD_SIZE, PLAYBACK_PERIOD_COUNT),
        .silence_threshold = 0,
        .silence_size = UINT_MAX,
        .avail_min = PLAYBACK_AVAILABLE_MIN,
    },
    .card = SOUND_CARD,
    .id = SOUND_PLAYBACK_DEVICE,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE|
               AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_EARPIECE|AUDIO_DEVICE_OUT_ALL_SCO,
};

static struct pcm_device_profile pcm_device_deep_buffer = {
    .config = {
        .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
        .rate = DEEP_BUFFER_OUTPUT_SAMPLING_RATE,
        .period_size = DEEP_BUFFER_OUTPUT_PERIOD_SIZE,
        .period_count = DEEP_BUFFER_OUTPUT_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
        .stop_threshold = INT_MAX,
        .avail_min = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
    },
    .card = SOUND_CARD,
    .id = SOUND_DEEP_BUFFER_DEVICE,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE|
               AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_EARPIECE|AUDIO_DEVICE_OUT_ALL_SCO,
};

static struct pcm_device_profile pcm_device_capture = {
    .config = {
        .channels = CAPTURE_DEFAULT_CHANNEL_COUNT,
        .rate = CAPTURE_DEFAULT_SAMPLING_RATE,
        .period_size = CAPTURE_PERIOD_SIZE,
        .period_count = CAPTURE_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = SOUND_CARD,
    .id = SOUND_CAPTURE_DEVICE,
    .type = PCM_CAPTURE,
    .devices = AUDIO_DEVICE_IN_BUILTIN_MIC|AUDIO_DEVICE_IN_WIRED_HEADSET|AUDIO_DEVICE_IN_BACK_MIC|AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
};

static struct pcm_device_profile pcm_device_playback_spk_and_headset = {
    .config = {
        .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
        .rate = PLAYBACK_DEFAULT_SAMPLING_RATE,
        .period_size = PLAYBACK_PERIOD_SIZE,
        .period_count = PLAYBACK_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = PLAYBACK_START_THRESHOLD(PLAYBACK_PERIOD_SIZE, PLAYBACK_PERIOD_COUNT),
        .stop_threshold = PLAYBACK_STOP_THRESHOLD(PLAYBACK_PERIOD_SIZE, PLAYBACK_PERIOD_COUNT),
        .silence_threshold = 0,
        .silence_size = UINT_MAX,
        .avail_min = PLAYBACK_AVAILABLE_MIN,
    },
    .card = SOUND_CARD,
    .id = SOUND_PLAYBACK_DEVICE,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
};

static struct pcm_device_profile * const pcm_devices[] = {
    &pcm_device_playback,
    &pcm_device_capture,
    &pcm_device_playback_spk_and_headset,
    NULL,
};

static const char * const use_case_table[AUDIO_USECASE_MAX] = {
    [USECASE_AUDIO_PLAYBACK] = "playback",
    [USECASE_AUDIO_PLAYBACK_MULTI_CH] = "playback multi-channel",
    [USECASE_AUDIO_PLAYBACK_DEEP_BUFFER] = "playback deep-buffer",
    [USECASE_AUDIO_CAPTURE] = "capture",
    [USECASE_VOICE_CALL] = "voice-call",
};

#define STRING_TO_ENUM(string) { #string, string }

static unsigned int audio_device_ref_count;

struct string_to_enum {
    const char *name;
    uint32_t value;
};

static const struct string_to_enum out_channels_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_5POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_7POINT1),
};

/* Array to store sound devices */
static const char * const device_table[SND_DEVICE_MAX] = {
    [SND_DEVICE_NONE] = "none",
    /* Playback sound devices */
    [SND_DEVICE_OUT_EARPIECE] = "route-earpiece",
    [SND_DEVICE_OUT_SPEAKER] = "route-speaker",
    [SND_DEVICE_OUT_HEADPHONES] = "route-headphones",
    [SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES] = "route-speaker-and-headphones",
    [SND_DEVICE_OUT_BT_SCO] = "route-bt-sco-headset",
    [SND_DEVICE_OUT_INCALL_EARPIECE] = "route-incall-earpiece",
    [SND_DEVICE_OUT_INCALL_EARPIECE_WB] = "route-incall-earpiece-wb",
    [SND_DEVICE_OUT_INCALL_SPEAKER] = "route-incall-speaker",
    [SND_DEVICE_OUT_INCALL_SPEAKER_WB] = "route-incall-speaker-wb",
    [SND_DEVICE_OUT_INCALL_HEADPHONES] = "route-incall-headphones",
    [SND_DEVICE_OUT_INCALL_HEADPHONES_WB] = "route-incall-headphones-wb",
    [SND_DEVICE_OUT_INCALL_BT_SCO] = "route-incall-bt-sco-headset",
    [SND_DEVICE_OUT_INCALL_BT_SCO_WB] = "route-incall-bt-sco-headset-wb",
    [SND_DEVICE_OUT_COMM_EARPIECE] = "route-comm-earpiece",
    [SND_DEVICE_OUT_COMM_SPEAKER] = "route-comm-speaker",
    [SND_DEVICE_OUT_COMM_HEADPHONES] = "route-comm-headphones",
    [SND_DEVICE_OUT_COMM_BT_SCO] = "route-comm-bt-sco-headset",

    /* Capture sound devices */
    [SND_DEVICE_IN_EARPIECE_MIC] = "route-earpiece-mic",
    [SND_DEVICE_IN_SPEAKER_MIC] = "route-speaker-mic",
    [SND_DEVICE_IN_HEADSET_MIC] = "route-headset-mic",
    [SND_DEVICE_IN_BT_SCO_MIC] = "route-bt-sco-mic",
    [SND_DEVICE_IN_INCALL_EARPIECE_MIC] = "route-incall-earpiece-mic",
    [SND_DEVICE_IN_INCALL_EARPIECE_MIC_WB] = "route-incall-earpiece-mic-wb",
    [SND_DEVICE_IN_INCALL_SPEAKER_MIC] = "route-incall-speaker-mic",
    [SND_DEVICE_IN_INCALL_SPEAKER_MIC_WB] = "route-incall-speaker-mic-wb",
    [SND_DEVICE_IN_INCALL_HEADSET_MIC] = "route-incall-headset-mic",
    [SND_DEVICE_IN_INCALL_HEADSET_MIC_WB] = "route-incall-headset-mic-wb",
    [SND_DEVICE_IN_INCALL_BT_SCO_MIC] = "route-incall-bt-sco-mic",
    [SND_DEVICE_IN_INCALL_BT_SCO_MIC_WB] = "route-incall-bt-sco-mic-wb",
    [SND_DEVICE_IN_COMM_EARPIECE_MIC] = "route-comm-earpiece-mic",
    [SND_DEVICE_IN_COMM_SPEAKER_MIC] = "route-comm-speaker-mic",
    [SND_DEVICE_IN_COMM_HEADSET_MIC] = "route-comm-headset-mic",
    [SND_DEVICE_IN_COMM_BT_SCO_MIC] = "route-comm-bt-sco-mic",
    [SND_DEVICE_IN_CAMCORDER_MIC] = "route-camcorder-mic",
    [SND_DEVICE_IN_CAMCORDER_HEADSET_MIC] = "route-camcorder-headset-mic",
    [SND_DEVICE_IN_VOICE_RECOGNITION_MIC] = "route-voice-recognition-mic",
    [SND_DEVICE_IN_VOICE_RECOGNITION_HEADSET_MIC] = "route-voice-recognition-headset-mic",
    [SND_DEVICE_IN_VOICE_RECOGNITION_BT_SCO_MIC] = "route-voice-recognition-bt-sco-headset-in",
};

static struct mixer_card *adev_get_mixer_for_card(struct audio_device *adev, int card)
{
    struct mixer_card *mixer_card;
    struct listnode *node;

    list_for_each(node, &adev->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, adev_list_node);
        if (mixer_card->card == card)
            return mixer_card;
    }
    return NULL;
}

static struct mixer_card *uc_get_mixer_for_card(struct audio_usecase *usecase, int card)
{
    struct mixer_card *mixer_card;
    struct listnode *node;

    list_for_each(node, &usecase->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, uc_list_node[usecase->id]);
        if (mixer_card->card == card)
            return mixer_card;
    }
    return NULL;
}

static void free_mixer_list(struct audio_device *adev)
{
    struct mixer_card *mixer_card;
    struct listnode *node;
    struct listnode *next;

    list_for_each_safe(node, next, &adev->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, adev_list_node);
        list_remove(node);
        audio_route_free(mixer_card->audio_route);
        free(mixer_card);
    }
}

static int mixer_init(struct audio_device *adev)
{
    int i;
    int card;
    int retry_num;
    struct mixer *mixer;
    struct audio_route *audio_route;
    char mixer_path[PATH_MAX];
    const char *mixer_type;
    struct mixer_card *mixer_card;
    int ret = 0;

    list_init(&adev->mixer_list);

    if (adev->audience_enabled) {
        mixer_type = "_audience";
    } else {
        mixer_type = "";
    } 
            
    for (i = 0; pcm_devices[i] != NULL; i++) {
        card = pcm_devices[i]->card;
        if (adev_get_mixer_for_card(adev, card) == NULL) {
            retry_num = 0;
            do {
                mixer = mixer_open(card);
                if (mixer == NULL) {
                    if (++retry_num > RETRY_NUMBER) {
                        ALOGE("%s unable to open the mixer for--card %d, aborting.",
                              __func__, card);
                        ret = -ENODEV;
                        goto error;
                    }
                    usleep(RETRY_US);
                }
            } while (mixer == NULL);

            sprintf(mixer_path, "/vendor/etc/mixer_paths%s.xml", mixer_type);
            if (access(mixer_path, F_OK) == -1) {
                ALOGW("%s: Failed to open mixer paths from %s, retrying with legacy location",
                      __func__, mixer_path);
                sprintf(mixer_path, "/system/etc/mixer_paths%s.xml", mixer_type);
                ALOGI("%s: Loading mixer paths from %s",__func__, mixer_path);
                if (access(mixer_path, F_OK) == -1) {
                    ALOGE("%s: Failed to load a mixer paths configuration, your system will crash",
                          __func__);
                } else {
                    ALOGI("%s: Successfully mixer paths from %s",__func__, mixer_path);
                }
            } else {
                ALOGI("%s: Successfully mixer paths from %s",__func__, mixer_path);
            }

            audio_route = audio_route_init(card, mixer_path);
            if (!audio_route) {
                ALOGE("%s: Failed to init audio route controls for card %d, aborting.",
                      __func__, card);
                ret = -ENODEV;
                goto error;
            }
            mixer_card = calloc(1, sizeof(struct mixer_card));
            if (mixer_card == NULL) {
                ret = -ENOMEM;
                goto error;
            }
            mixer_card->card = card;
            mixer_card->mixer = mixer;
            mixer_card->audio_route = audio_route;

            /* Do not sleep on first enable_snd_device() */
            mixer_card->dsp_poweroff_time.tv_sec = 1;
            mixer_card->dsp_poweroff_time.tv_nsec = 0;

            list_add_tail(&adev->mixer_list, &mixer_card->adev_list_node);
        }
    }

    return 0;

error:
    free_mixer_list(adev);
    return ret;
}

static const char *get_snd_device_name(snd_device_t snd_device)
{
    const char *name = NULL;

    if (snd_device == SND_DEVICE_NONE ||
        (snd_device >= SND_DEVICE_MIN && snd_device < SND_DEVICE_MAX))
        name = device_table[snd_device];

    ALOGE_IF(name == NULL, "%s: invalid snd device %d", __func__, snd_device);

   return name;
}

static const char *get_snd_device_display_name(snd_device_t snd_device)
{
    const char *name = get_snd_device_name(snd_device);

    if (name == NULL)
        name = "SND DEVICE NOT FOUND";

    return name;
}

static struct pcm_device_profile *get_pcm_device(usecase_type_t uc_type, audio_devices_t devices)
{
    int i;

    devices &= ~AUDIO_DEVICE_BIT_IN;

    if (!devices)
        return NULL;

    for (i = 0; pcm_devices[i] != NULL; i++) {
        if ((pcm_devices[i]->type == uc_type) &&
                (devices & pcm_devices[i]->devices) == devices)
            return pcm_devices[i];
    }

    return NULL;
}

static struct audio_usecase *get_usecase_from_id(struct audio_device *adev,
                                                   audio_usecase_t uc_id)
{
    struct audio_usecase *usecase;
    struct listnode *node;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->id == uc_id)
            return usecase;
    }
    return NULL;
}

static struct audio_usecase *get_usecase_from_type(struct audio_device *adev,
                                                        usecase_type_t type)
{
    struct audio_usecase *usecase;
    struct listnode *node;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->type & type)
            return usecase;
    }
    return NULL;
}

static snd_device_t get_output_snd_device(struct audio_device *adev, audio_devices_t devices)
{
    audio_mode_t mode = adev->mode;
    snd_device_t snd_device = SND_DEVICE_NONE;

    ALOGV("%s: enter: output devices(%#x), mode(%d)", __func__, devices, mode);
    if (devices == AUDIO_DEVICE_NONE ||
        devices & AUDIO_DEVICE_BIT_IN) {
        ALOGV("%s: Invalid output devices (%#x)", __func__, devices);
        goto exit;
    }

    if (mode == AUDIO_MODE_IN_CALL) {

        if (adev->wb_amr_type >= 1) {
            if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                snd_device = SND_DEVICE_OUT_INCALL_HEADPHONES_WB;
            } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
                snd_device = SND_DEVICE_OUT_INCALL_SPEAKER_WB;
            } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
                snd_device = SND_DEVICE_OUT_INCALL_EARPIECE_WB;
            }
        } else {
            if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                snd_device = SND_DEVICE_OUT_INCALL_HEADPHONES;
            } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
                snd_device = SND_DEVICE_OUT_INCALL_SPEAKER;
            } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
                snd_device = SND_DEVICE_OUT_INCALL_EARPIECE;
            }
        }
        if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
            if (adev->bluetooth_wb == true) {
                snd_device = SND_DEVICE_OUT_INCALL_BT_SCO_WB;
            } else {
                snd_device = SND_DEVICE_OUT_INCALL_BT_SCO;
            }
        }

        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (mode == AUDIO_MODE_IN_COMMUNICATION) {
        if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
            devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_OUT_COMM_HEADPHONES;
        } else if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
            snd_device = SND_DEVICE_OUT_COMM_BT_SCO;
        } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_OUT_COMM_SPEAKER;
        } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_OUT_COMM_EARPIECE;
        }
        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (popcount(devices) == 2) {
        if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                        AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADSET |
                               AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else {
            goto exit;
        }
        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (popcount(devices) != 1) {
        goto exit;
    }

    if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
        devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
        snd_device = SND_DEVICE_OUT_HEADPHONES;
    } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
        snd_device = SND_DEVICE_OUT_SPEAKER;
    } else if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
        snd_device = SND_DEVICE_OUT_BT_SCO;
    } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
        snd_device = SND_DEVICE_OUT_EARPIECE;
    } else {
        ALOGE("%s: Unknown device(s) %#x", __func__, devices);
    }
exit:
    ALOGV("%s: exit: snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}

static snd_device_t get_input_snd_device(struct audio_device *adev, audio_devices_t out_device)
{
    audio_source_t  source;
    audio_mode_t    mode   = adev->mode;
    audio_devices_t in_device;
    audio_channel_mask_t channel_mask;
    snd_device_t snd_device = SND_DEVICE_NONE;
    struct stream_in *active_input = NULL;
    struct audio_usecase *usecase;

    usecase = get_usecase_from_type(adev, PCM_CAPTURE|VOICE_CALL);
    if (usecase != NULL) {
        ALOGV("%s: get usecase from type PCM_CAPTURE or VOICECALL",__func__);
        active_input = (struct stream_in *)usecase->stream;
    }
    source = (active_input == NULL) ?
                                AUDIO_SOURCE_DEFAULT : active_input->source;

    in_device = ((active_input == NULL) ?
                                    AUDIO_DEVICE_NONE : active_input->devices)
                                & ~AUDIO_DEVICE_BIT_IN;
    channel_mask = (active_input == NULL) ?
                                AUDIO_CHANNEL_IN_MONO : active_input->main_channels;

    ALOGV("%s: enter: out_device(%#x) in_device(%#x)",
          __func__, out_device, in_device);

    if (mode == AUDIO_MODE_IN_CALL) {
        if (out_device == AUDIO_DEVICE_NONE) {
            ALOGE("%s: No output device set for voice call", __func__);
            goto exit;
        }
        if (adev->wb_amr_type >= 1) {
            if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                snd_device = SND_DEVICE_IN_INCALL_HEADSET_MIC_WB;
            } else if (out_device & AUDIO_DEVICE_OUT_EARPIECE ||
                out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                snd_device = SND_DEVICE_IN_INCALL_EARPIECE_MIC_WB;
            } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
                snd_device = SND_DEVICE_IN_INCALL_SPEAKER_MIC_WB;
            }
        } else { 
            if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                snd_device = SND_DEVICE_IN_INCALL_HEADSET_MIC;
            } else if (out_device & AUDIO_DEVICE_OUT_EARPIECE ||
                out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                snd_device = SND_DEVICE_IN_INCALL_EARPIECE_MIC;
            } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
                snd_device = SND_DEVICE_IN_INCALL_SPEAKER_MIC;
            }
	}
        /* BT SCO */
        if (out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
            if (adev->bluetooth_wb == true) {
                snd_device = SND_DEVICE_IN_INCALL_BT_SCO_MIC_WB;
            } else {
                snd_device = SND_DEVICE_IN_INCALL_BT_SCO_MIC;
            }
        }
    } else if (source == AUDIO_SOURCE_VOICE_COMMUNICATION || mode == AUDIO_MODE_IN_COMMUNICATION) {
        if (out_device & AUDIO_DEVICE_OUT_SPEAKER)
            in_device = AUDIO_DEVICE_IN_BACK_MIC;
        if (active_input) {
            if (out_device & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) {
                snd_device = SND_DEVICE_IN_COMM_BT_SCO_MIC;
            } else if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
                snd_device = SND_DEVICE_IN_COMM_SPEAKER_MIC;
            } else if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
                if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                   snd_device = SND_DEVICE_IN_COMM_SPEAKER_MIC;
                } else {
                    snd_device = SND_DEVICE_IN_COMM_EARPIECE_MIC;
                }
            } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
                snd_device = SND_DEVICE_IN_COMM_HEADSET_MIC;
            }
        }
    } else if (source == AUDIO_SOURCE_CAMCORDER) {
        if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_CAMCORDER_HEADSET_MIC;
        } else {
            snd_device = SND_DEVICE_IN_CAMCORDER_MIC;
        }
    } else if (source == AUDIO_SOURCE_VOICE_RECOGNITION) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            if (snd_device == SND_DEVICE_NONE) {
                snd_device = SND_DEVICE_IN_VOICE_RECOGNITION_MIC;
            }
        } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_VOICE_RECOGNITION_HEADSET_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            snd_device = SND_DEVICE_IN_VOICE_RECOGNITION_BT_SCO_MIC;
        }
    } else if (source == AUDIO_SOURCE_MIC) {
        if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_EARPIECE ||
            out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        }
    } else if (source == AUDIO_SOURCE_DEFAULT) {
        goto exit;
    }


    if (snd_device != SND_DEVICE_NONE) {
        goto exit;
    }

    if (in_device != AUDIO_DEVICE_NONE &&
            !(in_device & AUDIO_DEVICE_IN_VOICE_CALL) &&
            !(in_device & AUDIO_DEVICE_IN_COMMUNICATION)) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            snd_device = SND_DEVICE_IN_BT_SCO_MIC ;
        } else {
            ALOGW("%s: Using default earpiece-mic", __func__);
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        }
    } else {
        if (out_device & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) {
            snd_device = SND_DEVICE_IN_BT_SCO_MIC;
        } else {
            ALOGW("%s: Using default earpiece-mic", __func__);
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        }
    }
exit:
    ALOGV("%s: exit: in_snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}

/* Delay in Us */
static int64_t render_latency(audio_usecase_t usecase)
{
    (void)usecase;
    /* TODO */
    return 0;
}

static int enable_snd_device(struct audio_device *adev,
                             struct audio_usecase *uc_info,
                             snd_device_t snd_device)
{
    struct mixer_card *mixer_card;
    struct listnode *node;
    const char *snd_device_name = get_snd_device_name(snd_device);

    if (snd_device_name == NULL)
        return -EINVAL;

    adev->snd_dev_ref_cnt[snd_device]++;
    if (adev->snd_dev_ref_cnt[snd_device] > 1) {
        ALOGV("%s: snd_device(%d: %s) is already active",
              __func__, snd_device, snd_device_name);
        return 0;
    }

    ALOGV("%s: snd_device(%d: %s)", __func__,
          snd_device, snd_device_name);

    list_for_each(node, &uc_info->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, uc_list_node[uc_info->id]);
        audio_route_apply_and_update_path(mixer_card->audio_route, snd_device_name);
    }

    return 0;
}

int disable_snd_device(struct audio_device *adev,
                              struct audio_usecase *uc_info,
                              snd_device_t snd_device)
{
    struct mixer_card *mixer_card;
    struct listnode *node;
    struct audio_usecase *out_uc_info = get_usecase_from_type(adev, PCM_PLAYBACK);
    const char *snd_device_name = get_snd_device_name(snd_device);
    const char *out_snd_device_name = NULL;

    if (snd_device_name == NULL)
        return -EINVAL;

    if (adev->snd_dev_ref_cnt[snd_device] <= 0) {
        ALOGE("%s: device ref cnt is already 0", __func__);
        return -EINVAL;
    }
    adev->snd_dev_ref_cnt[snd_device]--;
    if (adev->snd_dev_ref_cnt[snd_device] == 0) {
        ALOGV("%s: snd_device(%d: %s)", __func__,
              snd_device, snd_device_name);
        list_for_each(node, &uc_info->mixer_list) {
            mixer_card = node_to_item(node, struct mixer_card, uc_list_node[uc_info->id]);
            audio_route_reset_and_update_path(mixer_card->audio_route, snd_device_name);
            if (snd_device > SND_DEVICE_IN_BEGIN && out_uc_info != NULL) {
                /*
                 * Cycle the rx device to eliminate routing conflicts.
                 * This prevents issues when an input route shares mixer controls with an output
                 * route.
                 */
                out_snd_device_name = get_snd_device_name(out_uc_info->out_snd_device);
                audio_route_apply_and_update_path(mixer_card->audio_route, out_snd_device_name);
            }
        }
    }
    return 0;
}

static int set_voice_volume(struct audio_device *adev, float volume)
{
    int ret = 0;
    enum _SoundType sound_type;

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        if (adev->primary_output->devices & AUDIO_DEVICE_OUT_ALL_SCO) {
            sound_type = SOUND_TYPE_BTVOICE;
        } else if (adev->primary_output->devices & AUDIO_DEVICE_OUT_EARPIECE) {
            sound_type = SOUND_TYPE_VOICE;
        } else if (adev->primary_output->devices & AUDIO_DEVICE_OUT_SPEAKER) {
            sound_type = SOUND_TYPE_SPEAKER;
        } else if ((adev->primary_output->devices & AUDIO_DEVICE_OUT_WIRED_HEADSET)
						|| (adev->primary_output->devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
            sound_type = SOUND_TYPE_HEADSET;
        } else {
               ALOGV("%s: warning default", __func__);
            sound_type = SOUND_TYPE_VOICE;
        }

        ALOGV("%s: ril_set_call_volume:(%d) volume:%f", __func__, sound_type, volume);

        ril_set_call_volume(&adev->ril, sound_type, volume);
    }
    return ret;
}


static void do_routing_for_call(struct audio_device *adev, audio_devices_t out_device)
{
    enum _AudioPath device_type;

    if ((out_device & AUDIO_DEVICE_OUT_EARPIECE) || (out_device & AUDIO_DEVICE_OUT_SPEAKER)) {
        ril_set_two_mic_control(&adev->ril, AUDIENCE, TWO_MIC_SOLUTION_ON);
    } else {
        ril_set_two_mic_control(&adev->ril, AUDIENCE, TWO_MIC_SOLUTION_OFF);
    }

    /* ril_set_call_audio_path */
    if (out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
        if (adev->bluetooth_wb == true) {
            if (adev->bluetooth_nrec == true) {
                device_type = SOUND_AUDIO_PATH_BLUETOOTH_WB;
            } else {
                device_type = SOUND_AUDIO_PATH_BLUETOOTH_WB_NO_NR;
            }
        } else {
            if (adev->bluetooth_nrec == true) {
                device_type = SOUND_AUDIO_PATH_BLUETOOTH;
            } else {
                device_type = SOUND_AUDIO_PATH_BLUETOOTH_NO_NR;
            }
        }
    } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
        device_type = SOUND_AUDIO_PATH_SPEAKER;
    } else if (out_device & AUDIO_DEVICE_OUT_EARPIECE) {
        device_type = SOUND_AUDIO_PATH_EARPIECE;
    } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
        device_type = SOUND_AUDIO_PATH_HEADSET;
    } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
        device_type = SOUND_AUDIO_PATH_HEADPHONE;
    } else {
        ALOGV("%s: warning default", __func__);
        device_type = SOUND_AUDIO_PATH_EARPIECE;
    }
    ril_set_call_audio_path(&adev->ril, device_type);

    set_voice_volume(adev, adev->volume);
}

static void check_and_route_usecases(struct audio_device *adev,
                                              struct audio_usecase *uc_info,
                                              usecase_type_t type,
                                              snd_device_t snd_device)
{
    struct listnode *node;
    struct audio_usecase *usecase;
    bool switch_device[AUDIO_USECASE_MAX], need_switch = false;
    snd_device_t usecase_snd_device = SND_DEVICE_NONE;
    int i;

    /*
     * This function is to make sure that all the usecases that are active on
     * the hardware codec backend are always routed to any one device that is
     * handled by the hardware codec.
     * For example, if low-latency and deep-buffer usecases are currently active
     * on speaker and out_set_parameters(headset) is received on low-latency
     * output, then we have to make sure deep-buffer is also switched to headset or
     * if audio-record and voice-call usecases are currently
     * active on speaker(rx) and speaker-mic (tx) and out_set_parameters(earpiece)
     * is received for voice call then we have to make sure that audio-record
     * usecase is also switched to earpiece i.e.
     * because of the limitation that both the devices cannot be enabled
     * at the same time as they share the same backend.
     */
    /* Disable all the usecases on the shared backend other than the
       specified usecase */
    for (i = 0; i < AUDIO_USECASE_MAX; i++)
        switch_device[i] = false;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->type != type || usecase == uc_info)
            continue;
        usecase_snd_device = (type == PCM_PLAYBACK) ? usecase->out_snd_device :
                              usecase->in_snd_device;
        if (usecase_snd_device != snd_device) {
            ALOGV("%s: Usecase (%s) is active on (%s) - disabling ..",
                  __func__, use_case_table[usecase->id],
                  get_snd_device_name(usecase_snd_device));
            switch_device[usecase->id] = true;
            need_switch = true;
        }
    }
    if (need_switch) {
        list_for_each(node, &adev->usecase_list) {
            usecase = node_to_item(node, struct audio_usecase, adev_list_node);
            usecase_snd_device = (type == PCM_PLAYBACK) ? usecase->out_snd_device :
                                  usecase->in_snd_device;
            if (switch_device[usecase->id]) {
                disable_snd_device(adev, usecase, usecase_snd_device);
                enable_snd_device(adev, usecase, snd_device);
                if (type == PCM_PLAYBACK)
                    usecase->out_snd_device = snd_device;
                else
                    usecase->in_snd_device = snd_device;
            }
        }
    }
}

static int select_devices(struct audio_device *adev,
                          audio_usecase_t uc_id)
{
    snd_device_t out_snd_device = SND_DEVICE_NONE;
    snd_device_t in_snd_device = SND_DEVICE_NONE;
    struct audio_usecase *usecase = NULL;
    struct audio_usecase *vc_usecase = NULL;
    struct stream_in *active_input = NULL;
    struct stream_out *active_out;

    ALOGV("%s: usecase(%d)", __func__, uc_id);

    usecase = get_usecase_from_type(adev, PCM_CAPTURE|VOICE_CALL);
    if (usecase != NULL) {
        active_input = (struct stream_in *)usecase->stream;
    }

    usecase = get_usecase_from_id(adev, uc_id);
    if (usecase == NULL) {
        ALOGE("%s: Could not find the usecase(%d)", __func__, uc_id);
        return -EINVAL;
    }
    active_out = (struct stream_out *)usecase->stream;

    if (usecase->type == VOICE_CALL) {
        in_snd_device = get_input_snd_device(adev, active_out->devices);
        out_snd_device = get_output_snd_device(adev, active_out->devices);
        usecase->devices = active_out->devices;
    } else {
        /*
         * If the voice call is active, use the sound devices of voice call usecase
         * so that it would not result any device switch. All the usecases will
         * be switched to new device when select_devices() is called for voice call
         * usecase.
         */
        if (adev->in_call && adev->mode == AUDIO_MODE_IN_CALL) {
            vc_usecase = get_usecase_from_id(adev, USECASE_VOICE_CALL);
            if (vc_usecase) {
                in_snd_device = vc_usecase->in_snd_device;
                out_snd_device = vc_usecase->out_snd_device;
            }
        }
        if (usecase->type == PCM_PLAYBACK) {
            usecase->devices = active_out->devices;
            in_snd_device = SND_DEVICE_NONE;
            if (out_snd_device == SND_DEVICE_NONE) {
                out_snd_device = get_output_snd_device(adev, active_out->devices);
                if (active_out == adev->primary_output &&
                        active_input &&
                        active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION) {
                    select_devices(adev, active_input->usecase);
                }
            }
        } else if (usecase->type == PCM_CAPTURE) {
            usecase->devices = ((struct stream_in *)usecase->stream)->devices;
            out_snd_device = SND_DEVICE_NONE;
            if (in_snd_device == SND_DEVICE_NONE) {
                if (active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION &&
                        adev->primary_output && !adev->primary_output->standby) {
                    in_snd_device = get_input_snd_device(adev, adev->primary_output->devices);
                } else {
                    in_snd_device = get_input_snd_device(adev, AUDIO_DEVICE_NONE);
                }
            }
        }
    }
    if (out_snd_device == usecase->out_snd_device &&
        in_snd_device == usecase->in_snd_device) {
        return 0;
    }

    ALOGV("%s: out_snd_device(%d: %s) in_snd_device(%d: %s)", __func__,
          out_snd_device, get_snd_device_display_name(out_snd_device),
          in_snd_device,  get_snd_device_display_name(in_snd_device));

    if ((usecase->type == VOICE_CALL) || adev->in_call) {
        do_routing_for_call(adev, active_out->devices);
    }

    /* Disable current sound devices */
    if (usecase->out_snd_device != SND_DEVICE_NONE) {
        disable_snd_device(adev, usecase, usecase->out_snd_device);
    }

    if (usecase->in_snd_device != SND_DEVICE_NONE) {
        disable_snd_device(adev, usecase, usecase->in_snd_device);
    }

    /* Enable new sound devices */
    if (out_snd_device != SND_DEVICE_NONE) {
        check_and_route_usecases(adev, usecase, PCM_PLAYBACK, out_snd_device);
        enable_snd_device(adev, usecase, out_snd_device);
    }

    if (in_snd_device != SND_DEVICE_NONE) {
        check_and_route_usecases(adev, usecase, PCM_CAPTURE, in_snd_device);
        enable_snd_device(adev, usecase, in_snd_device);
    }

    if ((usecase->type == VOICE_CALL) || adev->in_call) {
        ril_set_mute(&adev->ril, RX_UNMUTE);
        if (!adev->mic_mute) {
            ril_set_mute(&adev->ril, TX_UNMUTE);
        }
    }

    usecase->in_snd_device = in_snd_device;
    usecase->out_snd_device = out_snd_device;

    return 0;
}

static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames);
static int do_in_standby_l(struct stream_in *in);
static audio_format_t in_get_format(const struct audio_stream *stream);


/* This function reads PCM data and:
 * - resample if needed
 * - process if pre-processors are attached
 * - discard unwanted channels
 */
static ssize_t read_and_process_frames(struct audio_stream_in *stream, void* buffer, ssize_t frames_num)
{
    struct stream_in *in = (struct stream_in *)stream;
    ssize_t frames_wr = 0; /* Number of frames actually read */
    size_t bytes_per_sample = audio_bytes_per_sample(stream->common.get_format(&stream->common));
    void *proc_buf_out = buffer;

    /* Additional channels might be added on top of main_channels:
    * - aux_channels (by processing effects)
    * - extra channels due to HW limitations
    * In case of additional channels, we cannot work inplace
    */
    size_t src_channels = in->config.channels;
    size_t dst_channels = audio_channel_count_from_in_mask(in->main_channels);
    bool channel_remapping_needed = (dst_channels != src_channels);
    const size_t src_frame_size = src_channels * bytes_per_sample;

    /* With additional channels or processing, we need intermediate buffers */
    if (channel_remapping_needed) {
        const size_t src_buffer_size = frames_num * src_frame_size;

        if (in->proc_buf_size < src_buffer_size) {
            in->proc_buf_size = src_buffer_size;
            in->proc_buf_out = realloc(in->proc_buf_out, src_buffer_size);
            ALOG_ASSERT((in->proc_buf_out != NULL),
                    "process_frames() failed to reallocate proc_buf_out");
        }
        if (channel_remapping_needed) {
            proc_buf_out = in->proc_buf_out;
        }
    }

    frames_wr = read_frames(in, proc_buf_out, frames_num);
    ALOG_ASSERT(frames_wr <= frames_num, "read more frames than requested");

    /* check negative frames_wr (error) before channel remapping to avoid overwriting memory. */
    if (channel_remapping_needed && frames_wr > 0) {
        adjust_channels(proc_buf_out, src_channels, buffer, dst_channels,
            bytes_per_sample, frames_wr * src_frame_size);
    }

    return frames_wr;
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer)
{
    struct stream_in *in;
    struct pcm_device *pcm_device;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    if (list_empty(&in->pcm_dev_list)) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    if (in->read_buf_frames == 0) {
        size_t size_in_bytes = pcm_frames_to_bytes(pcm_device->pcm, in->config.period_size);
        if (in->read_buf_size < in->config.period_size) {
            in->read_buf_size = in->config.period_size;
            in->read_buf = (int16_t *) realloc(in->read_buf, size_in_bytes);
            ALOG_ASSERT((in->read_buf != NULL),
                        "get_next_buffer() failed to reallocate read_buf");
        }

        in->read_status = pcm_read(pcm_device->pcm, (void*)in->read_buf, size_in_bytes);

        if (in->read_status != 0) {
            ALOGE("get_next_buffer() pcm_read error %d", in->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }
        in->read_buf_frames = in->config.period_size;
    }

    buffer->frame_count = (buffer->frame_count > in->read_buf_frames) ?
                                in->read_buf_frames : buffer->frame_count;
    buffer->i16 = in->read_buf + (in->config.period_size - in->read_buf_frames) *
                                                in->config.channels;
    return in->read_status;
}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                                  struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    in->read_buf_frames -= buffer->frame_count;
}

/* read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified */
static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;

    struct pcm_device *pcm_device;

    if (list_empty(&in->pcm_dev_list)) {
        ALOGE("%s: pcm device list empty", __func__);
        return -EINVAL;
    }

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        ALOGVV("%s: frames_rd: %zd, frames_wr: %zd, in->config.channels: %d",
               __func__,frames_rd,frames_wr,in->config.channels);
        if (in->resampler != NULL) {
            in->resampler->resample_from_provider(in->resampler,
                    (int16_t *)((char *)buffer +
                            pcm_frames_to_bytes(pcm_device->pcm, frames_wr)),
                    &frames_rd);
        } else {
            struct resampler_buffer buf = {
                    .raw = NULL,
                    .frame_count = frames_rd,
            };
            get_next_buffer(&in->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                            pcm_frames_to_bytes(pcm_device->pcm, frames_wr),
                        buf.raw,
                        pcm_frames_to_bytes(pcm_device->pcm, buf.frame_count));
                frames_rd = buf.frame_count;
            }
            release_buffer(&in->buf_provider, &buf);
        }
        /* in->read_status is updated by getNextBuffer() also called by
         * in->resampler->resample_from_provider() */
        if (in->read_status != 0)
            return in->read_status;

        frames_wr += frames_rd;
    }
    return frames_wr;
}

static int in_release_pcm_devices(struct stream_in *in)
{
    struct pcm_device *pcm_device;
    struct listnode *node;
    struct listnode *next;

    list_for_each_safe(node, next, &in->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        list_remove(node);
        free(pcm_device);
    }
    ALOGV("%s: enter", __func__);

    return 0;
}

static int stop_input_stream(struct stream_in *in)
{
    struct audio_usecase *uc_info;
    struct audio_device *adev = in->dev;

    adev->active_input = NULL;
    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          in->usecase, use_case_table[in->usecase]);
    uc_info = get_usecase_from_id(adev, in->usecase);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
              __func__, in->usecase);
        return -EINVAL;
    }

    /* Disable the tx device */
    disable_snd_device(adev, uc_info, uc_info->in_snd_device);

    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    if (list_empty(&in->pcm_dev_list)) {
        ALOGE("%s: pcm device list empty", __func__);
        return -EINVAL;
    }

    in_release_pcm_devices(in);
    list_init(&in->pcm_dev_list);

    return 0;
}

static int start_input_stream(struct stream_in *in)
{
    /* Enable output device and stream routing controls */
    int ret = 0;
    bool recreate_resampler = false;
    struct audio_usecase *uc_info;
    struct audio_device *adev = in->dev;
    struct pcm_device_profile *pcm_profile;
    struct pcm_device *pcm_device;

    ALOGV("%s: enter: usecase(%d)", __func__, in->usecase);
    adev->active_input = in;
    pcm_profile = get_pcm_device(in->usecase_type, in->devices);
    if (pcm_profile == NULL) {
        ALOGE("%s: Could not find PCM device id for the usecase(%d)",
              __func__, in->usecase);
        ret = -EINVAL;
        goto error_config;
    }

    if (in->input_flags & AUDIO_INPUT_FLAG_FAST) {
        ALOGV("%s: change capture period size to low latency size %d",
              __func__, CAPTURE_PERIOD_SIZE_LOW_LATENCY);
        pcm_profile->config.period_size = CAPTURE_PERIOD_SIZE_LOW_LATENCY;
    }

    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    if (uc_info == NULL) {
        ALOGV("%s: uc_info is null", __func__);
        ret = -ENOMEM;
        goto error_config;
    }
    uc_info->id = in->usecase;
    uc_info->type = PCM_CAPTURE;
    uc_info->stream = (struct audio_stream *)in;
    uc_info->devices = in->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;

    pcm_device = (struct pcm_device *)calloc(1, sizeof(struct pcm_device));
    if (pcm_device == NULL) {
        ALOGV("%s: pcm_device is null", __func__);
        free(uc_info);
        ret = -ENOMEM;
        goto error_config;
    }

    pcm_device->pcm_profile = pcm_profile;
    list_init(&in->pcm_dev_list);
    list_add_tail(&in->pcm_dev_list, &pcm_device->stream_list_node);

    list_init(&uc_info->mixer_list);
    list_add_tail(&uc_info->mixer_list,
                  &adev_get_mixer_for_card(adev,
                                       pcm_device->pcm_profile->card)->uc_list_node[uc_info->id]);

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    select_devices(adev, in->usecase);

    /* Config should be updated as profile can be changed between different calls
     * to this function:
     * - Trigger resampler creation
     * - Config needs to be updated */
    if (in->config.rate != pcm_profile->config.rate) {
        recreate_resampler = true;
    }
    in->config = pcm_profile->config;

    if (in->requested_rate != in->config.rate) {
        recreate_resampler = true;
    }

    if (recreate_resampler) {
        if (in->resampler) {
            release_resampler(in->resampler);
            in->resampler = NULL;
        }
        in->buf_provider.get_next_buffer = get_next_buffer;
        in->buf_provider.release_buffer = release_buffer;
        ret = create_resampler(in->config.rate,
                               in->requested_rate,
                               in->config.channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               &in->buf_provider,
                               &in->resampler);
    }

    /* Open the PCM device.
     * The HW is limited to support only the default pcm_profile settings.
     * As such a change in aux_channels will not have an effect.
     */
    ALOGV("%s: Opening PCM device card_id(%d) device_id(%d), channels %d, smp rate %d format %d, \
          period_size %d", __func__, pcm_device->pcm_profile->card, pcm_device->pcm_profile->id,
          pcm_device->pcm_profile->config.channels,pcm_device->pcm_profile->config.rate,
          pcm_device->pcm_profile->config.format, pcm_device->pcm_profile->config.period_size);

    pcm_device->pcm = pcm_open(pcm_device->pcm_profile->card, pcm_device->pcm_profile->id,
                               PCM_IN | PCM_MONOTONIC, &pcm_device->pcm_profile->config);
    if (pcm_device->pcm && !pcm_is_ready(pcm_device->pcm)) {
        ALOGE("%s: %s", __func__, pcm_get_error(pcm_device->pcm));
        pcm_close(pcm_device->pcm);
        pcm_device->pcm = NULL;
        ret = -EIO;
        goto error_open;
    }

    /* force read and proc buffer reallocation in case of frame size or
     * channel count change */
    in->proc_buf_size = 0;
    in->read_buf_size = 0;
    in->read_buf_frames = 0;

    /* if no supported sample rate is available, use the resampler */
    if (in->resampler) {
        in->resampler->reset(in->resampler);
    }

    return ret;

error_open:
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }
    stop_input_stream(in);

error_config:
    adev->active_input = NULL;
    return ret;
}

void lock_input_stream(struct stream_in *in)
{
    pthread_mutex_lock(&in->pre_lock);
    pthread_mutex_lock(&in->lock);
    pthread_mutex_unlock(&in->pre_lock);
}

void lock_output_stream(struct stream_out *out)
{
    pthread_mutex_lock(&out->pre_lock);
    pthread_mutex_lock(&out->lock);
    pthread_mutex_unlock(&out->pre_lock);
}

static int uc_release_pcm_devices(struct audio_usecase *usecase)
{
    struct stream_out *out = (struct stream_out *)usecase->stream;
    struct pcm_device *pcm_device;
    struct listnode *node;
    struct listnode *next;

    list_for_each_safe(node, next, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        list_remove(node);
        free(pcm_device);
    }
    list_init(&usecase->mixer_list);

    return 0;
}

static int uc_select_pcm_devices(struct audio_usecase *usecase)

{
    struct stream_out *out = (struct stream_out *)usecase->stream;
    struct pcm_device *pcm_device;
    struct pcm_device_profile *pcm_profile;
    struct mixer_card *mixer_card;
    audio_devices_t devices = usecase->devices;

    list_init(&usecase->mixer_list);
    list_init(&out->pcm_dev_list);

    pcm_profile = get_pcm_device(usecase->type, devices);
    if (pcm_profile) {
        pcm_device = calloc(1, sizeof(struct pcm_device));
        pcm_device->pcm_profile = pcm_profile;
        list_add_tail(&out->pcm_dev_list, &pcm_device->stream_list_node);
        mixer_card = uc_get_mixer_for_card(usecase, pcm_profile->card);
        if (mixer_card == NULL) {
            mixer_card = adev_get_mixer_for_card(out->dev, pcm_profile->card);
            list_add_tail(&usecase->mixer_list, &mixer_card->uc_list_node[usecase->id]);
        }
        devices &= ~pcm_profile->devices;
    } else {
        ALOGE("usecase type=%d, devices=%d did not find exact match",
            usecase->type, devices);
    }

    return 0;
}

static int out_close_pcm_devices(struct stream_out *out)
{
    struct pcm_device *pcm_device;
    struct listnode *node;

    list_for_each(node, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        if (pcm_device->pcm) {
            pcm_close(pcm_device->pcm);
            pcm_device->pcm = NULL;
        }
    }

    return 0;
}

static int out_open_pcm_devices(struct stream_out *out)
{
    struct pcm_device *pcm_device;
    struct listnode *node;
    int ret = 0;
    int pcm_device_card;
    int pcm_device_id;

    list_for_each(node, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        pcm_device_card = pcm_device->pcm_profile->card;
        pcm_device_id = pcm_device->pcm_profile->id;

        if (out->flags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER)
            pcm_device_id = pcm_device_deep_buffer.id;

        ALOGV("%s: Opening PCM device card_id(%d) device_id(%d)",
              __func__, pcm_device_card, pcm_device_id);

        pcm_device->pcm = pcm_open(pcm_device_card, pcm_device_id,
                               PCM_OUT | PCM_MONOTONIC, &out->config);

        if (pcm_device->pcm && !pcm_is_ready(pcm_device->pcm)) {
            ALOGE("%s: %s", __func__, pcm_get_error(pcm_device->pcm));
            pcm_device->pcm = NULL;
            ret = -EIO;
            goto error_open;
        }
    }
    return ret;

error_open:
    out_close_pcm_devices(out);
    return ret;
}

int disable_output_path_l(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    struct audio_usecase *uc_info;

    uc_info = get_usecase_from_id(adev, out->usecase);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
             __func__, out->usecase);
        return -EINVAL;
    }
    disable_snd_device(adev, uc_info, uc_info->out_snd_device);
    uc_release_pcm_devices(uc_info);
    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    return 0;
}

int enable_output_path_l(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    struct audio_usecase *uc_info;

    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    if (uc_info == NULL) {
        ALOGV("%s: uc_info is null", __func__);
        return -ENOMEM;
    }

    uc_info->id = out->usecase;
    uc_info->type = PCM_PLAYBACK;
    uc_info->stream = (struct audio_stream *)out;
    uc_info->devices = out->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;
    uc_select_pcm_devices(uc_info);

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);
    select_devices(adev, out->usecase);

    return 0;
}

static int stop_output_stream(struct stream_out *out)
{
    int ret = 0;

    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          out->usecase, use_case_table[out->usecase]);

    ret = disable_output_path_l(out);

    ALOGV("%s: exit: status(%d)", __func__, ret);
    return ret;
}

static int start_output_stream(struct stream_out *out)
{
    int ret = 0;

    ALOGV("%s: enter: usecase(%d: %s) devices(%#x) channels(%d)",
          __func__, out->usecase, use_case_table[out->usecase], out->devices, out->config.channels);

    ret = enable_output_path_l(out);
    if (ret != 0) {
        ALOGV("%s: enable_output_path_l failed", __func__);
        goto error_config;
    }

    ret = out_open_pcm_devices(out);
    if (ret != 0)
        goto error_open;
    return 0;
error_open:
    stop_output_stream(out);
error_config:
    return ret;
}

void stop_voice_session_bt_sco(struct audio_device *adev) {
    ALOGV("%s: Closing SCO PCMs", __func__);
    if (adev->pcm_sco_rx != NULL) {
        pcm_stop(adev->pcm_sco_rx);
        pcm_close(adev->pcm_sco_rx);
        adev->pcm_sco_rx = NULL;
        ALOGV("%s: stopped pcm_sco_rx ", __func__);
    }
    if (adev->pcm_sco_tx != NULL) {
        pcm_stop(adev->pcm_sco_tx);
        pcm_close(adev->pcm_sco_tx);
        adev->pcm_sco_tx = NULL;
        ALOGV("%s: stopped pcm_sco_tx ", __func__);
    }
    adev->in_call_sco = false;
}

int stop_voice_call(struct audio_device *adev)
{
    struct audio_usecase *uc_info;
    ALOGV("%s: Closing Voice PCMs", __func__);

    if (adev->in_call_sco) {
        stop_voice_session_bt_sco(adev);
    }

    if (adev->pcm_voice_rx != NULL) {
        pcm_stop(adev->pcm_voice_rx);
        pcm_close(adev->pcm_voice_rx);
        adev->pcm_voice_rx = NULL;
        ALOGV("%s: stopped pcm_voice_rx ", __func__);
    }

    if (adev->pcm_voice_tx != NULL) {
        pcm_stop(adev->pcm_voice_tx);
        pcm_close(adev->pcm_voice_tx);
        adev->pcm_voice_tx = NULL;
        ALOGV("%s: stopped pcm_voice_tx ", __func__);
    }

    if (adev->audience_enabled) {
        ALOGV("%s: Stop voice session Audience IC", __func__);
        es_stop_voice_session();
    }

    uc_info = get_usecase_from_id(adev, USECASE_VOICE_CALL);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
              __func__, USECASE_VOICE_CALL);
        return -EINVAL;
    }

    disable_snd_device(adev, uc_info, uc_info->out_snd_device);
    disable_snd_device(adev, uc_info, uc_info->in_snd_device);

    list_remove(&uc_info->adev_list_node);
    free(uc_info);
    adev->in_call = false;

    ALOGV("%s: exit", __func__);
    return 0;
}

void start_voice_session_bt_sco(struct audio_device *adev)
{
    struct pcm_config *voice_sco_config;
    ALOGI("%s: Enter:)", __func__);
    if (adev->in_call_sco) {
        ALOGW("%s: BT session already started!\n", __func__);
        return;
    }
    if (adev->pcm_sco_rx != NULL || adev->pcm_sco_tx != NULL) {
        ALOGW("%s: SCO PCMs already open!\n", __func__);
        return;
    }
    if (adev->in_comm_sco) {
        stop_voice_session_bt_sco(adev);
    }

    adev->in_call_sco = true;
    ALOGV("%s: Opening SCO PCMs", __func__);

    if (adev->bluetooth_wb == true) {
        ALOGV("%s: pcm_config wideband", __func__);
        voice_sco_config = &pcm_config_voice_sco_wb;
    } else {
        ALOGV("%s: pcm_config narrowband", __func__);
        voice_sco_config = &pcm_config_voice_sco;
    }

    adev->pcm_sco_tx = pcm_open(SOUND_CARD,
                                   SOUND_CAPTURE_SCO_DEVICE,
                                   PCM_IN|PCM_MONOTONIC,
                                   voice_sco_config);
    if (adev->pcm_sco_tx && !pcm_is_ready(adev->pcm_sco_tx)) {
        ALOGE("%s: cannot open PCM SCO TX stream: %s",
              __func__, pcm_get_error(adev->pcm_sco_tx));
        goto err_sco_tx;
    }

    adev->pcm_sco_rx = pcm_open(SOUND_CARD,
                                   SOUND_PLAYBACK_SCO_DEVICE,
                                   PCM_OUT|PCM_MONOTONIC,
                                   voice_sco_config);
    if (adev->pcm_sco_rx != NULL && !pcm_is_ready(adev->pcm_sco_rx)) {
        ALOGE("%s: cannot open PCM SCO RX stream: %s",
              __func__, pcm_get_error(adev->pcm_sco_rx));
        goto err_sco_rx;
    }

    pcm_start(adev->pcm_sco_tx);
    pcm_start(adev->pcm_sco_rx);


    return;

err_sco_rx:
    pcm_close(adev->pcm_sco_rx);
    adev->pcm_sco_rx = NULL;
err_sco_tx:
    pcm_close(adev->pcm_sco_tx);
    adev->pcm_sco_tx = NULL;
    adev->in_call_sco = false;
}


/* always called with adev lock held */
int start_voice_call(struct audio_device *adev)
{
    struct audio_usecase *uc_info;

    int ret = 0;

   uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    if (uc_info == NULL) {
        ALOGV("%s: uc_info is null", __func__);
        ret = -ENOMEM;
        goto exit;
    }

    uc_info->id = USECASE_VOICE_CALL;
    uc_info->type = VOICE_CALL;
    uc_info->stream = (struct audio_stream *)adev->primary_output;
    uc_info->devices = adev->primary_output->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;

    list_init(&uc_info->mixer_list);
    list_add_tail(&uc_info->mixer_list,
                  &adev_get_mixer_for_card(adev, SOUND_CARD)->uc_list_node[uc_info->id]);

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    select_devices(adev, USECASE_VOICE_CALL);

    adev->in_call = true;

    /* start voice session */
    if (adev->pcm_voice_rx != NULL || adev->pcm_voice_tx != NULL) {
        ALOGW("%s: Voice PCMs already open!\n", __func__);
        return 0;
    }

    if (adev->primary_output->devices & AUDIO_DEVICE_OUT_ALL_SCO) {
        start_voice_session_bt_sco(adev);
    }

    adev->pcm_voice_tx = pcm_open(SOUND_CARD, SOUND_CAPTURE_VOICE_DEVICE,
                                     PCM_IN, &pcm_config_voicecall);
    if (adev->pcm_voice_tx != NULL && !pcm_is_ready(adev->pcm_voice_tx)) {
        ALOGE("%s: cannot open PCM voice TX stream: %s",
              __func__, pcm_get_error(adev->pcm_voice_tx));
        goto exit;
    }

    adev->pcm_voice_rx = pcm_open(SOUND_CARD, SOUND_PLAYBACK_VOICE_DEVICE,
                                     PCM_OUT, &pcm_config_voicecall);
    if (adev->pcm_voice_rx != NULL && !pcm_is_ready(adev->pcm_voice_rx)) {
        ALOGE("%s: cannot open PCM voice RX stream: %s",
              __func__, pcm_get_error(adev->pcm_voice_rx));
        goto exit;
    }

    pcm_start(adev->pcm_voice_tx);
    pcm_start(adev->pcm_voice_rx);

    if (adev->audience_enabled) {
        ALOGV("%s: Start voice session Audience IC", __func__);
        es_start_voice_session(adev);
    }

    return 0;

exit:
    ALOGV("%s: exit with error", __func__);
    pcm_close(adev->pcm_voice_rx);
    adev->pcm_voice_rx = NULL;
    pcm_close(adev->pcm_voice_tx);
    adev->pcm_voice_tx = NULL;
    if (adev->in_call_sco) {
        stop_voice_session_bt_sco(adev);
    }
    ril_set_call_clock_sync(&adev->ril, SOUND_CLOCK_STOP);
    adev->in_call = false;
    return ret;
}

static int check_input_parameters(uint32_t sample_rate,
                                  audio_format_t format,
                                  int channel_count)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT) return -EINVAL;

    if ((channel_count < 1) || (channel_count > 2)) return -EINVAL;

    switch (sample_rate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        ALOGV("%s: unknown sample rate", __func__);
        return -EINVAL;
    }

    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate,
                                    audio_format_t format,
                                    int channel_count,
                                    usecase_type_t usecase_type,
                                    audio_devices_t devices)
{
    size_t size = 0;
    struct pcm_device_profile *pcm_profile;

    if (check_input_parameters(sample_rate, format, channel_count) != 0)
        return 0;

    pcm_profile = get_pcm_device(usecase_type, devices);
    if (pcm_profile == NULL)
        return 0;

    /*
     * take resampling into account and return the closest majoring
     * multiple of 16 frames, as audioflinger expects audio buffers to
     * be a multiple of 16 frames
     */
    size = (pcm_profile->config.period_size * sample_rate) / pcm_profile->config.rate;
    size = ((size + 15) / 16) * 16;

    return (size * channel_count * audio_bytes_per_sample(format));

}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    (void)stream;
    (void)rate;
    return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->config.period_size *
               audio_stream_out_frame_size((const struct audio_stream_out *)stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    (void)stream;
    (void)format;
    return -ENOSYS;
}

static int do_out_standby_l(struct stream_out *out)
{
    int status = 0;

    out->standby = true;
    out_close_pcm_devices(out);
    status = stop_output_stream(out);

    return status;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;

    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          out->usecase, use_case_table[out->usecase]);
    lock_output_stream(out);
    if (!out->standby) {
        pthread_mutex_lock(&adev->lock);
        do_out_standby_l(out);
        pthread_mutex_unlock(&adev->lock);
    }
    pthread_mutex_unlock(&out->lock);
    ALOGV("%s: exit", __func__);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;

    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct str_parms *parms;
    char value[32];
    int ret, val = 0;
    bool devices_changed;

    ALOGV("%s: enter: usecase(%d: %s) kvpairs: %s out->devices(%#x) "
          "adev->mode(%#x)",
          __func__, out->usecase, use_case_table[out->usecase], kvpairs,
          out->devices, adev->mode);

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);

        ALOGV("%s: routing: usecase(%d: %s) devices=(%#x) adev->mode(%#x)",
              __func__, out->usecase, use_case_table[out->usecase], val,
              adev->mode);

        pthread_mutex_lock(&adev->lock_inputs);
        lock_output_stream(out);
        pthread_mutex_lock(&adev->lock);
        if (val != SND_DEVICE_NONE) {
            devices_changed = out->devices != (audio_devices_t)val;
            out->devices = val;

            if (!out->standby) {
                if (devices_changed)
                    do_out_standby_l(out);
                else {
                    select_devices(adev, out->usecase);
                }
            }
            /* Turn on bluetooth sco if needed */
            if (adev->mode == AUDIO_MODE_IN_COMMUNICATION &&
                (out->devices & AUDIO_DEVICE_OUT_ALL_SCO) && !adev->in_call_sco && !adev->in_comm_sco) {
                adev->in_comm_sco = true;
                start_voice_session_bt_sco(adev);
            } else if (!(out->devices & AUDIO_DEVICE_OUT_ALL_SCO) && adev->in_call_sco) {
                adev->in_comm_sco = false;
                stop_voice_session_bt_sco(adev);
            }

            if ((adev->mode == AUDIO_MODE_IN_CALL) && !adev->in_call &&
                    (out == adev->primary_output)) {
                start_voice_call(adev);
            } else if ((adev->mode == AUDIO_MODE_IN_CALL) && devices_changed &&
                       adev->in_call && (out == adev->primary_output)) {
                stop_voice_call(adev);
                start_voice_call(adev);
            }
        }

        pthread_mutex_unlock(&adev->lock);
        pthread_mutex_unlock(&out->lock);
        pthread_mutex_unlock(&adev->lock_inputs);
    }

    str_parms_destroy(parms);
    return 0;
}

static char* out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    size_t i, j;
    int ret;
    bool first = true;
    ALOGV("%s: enter: keys - %s", __func__, keys);
    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value, sizeof(value));
    if (ret >= 0) {
        value[0] = '\0';
        i = 0;
        while (out->supported_channel_masks[i] != 0) {
            for (j = 0; j < ARRAY_SIZE(out_channels_name_to_enum_table); j++) {
                if (out_channels_name_to_enum_table[j].value == out->supported_channel_masks[i]) {
                    if (!first) {
                        strcat(value, "|");
                    }
                    strcat(value, out_channels_name_to_enum_table[j].name);
                    first = false;
                    break;
                }
            }
            i++;
        }
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value);
        str = str_parms_to_str(reply);
    } else {
        str = strdup(keys);
    }
    str_parms_destroy(query);
    str_parms_destroy(reply);
    ALOGV("%s: exit: returns - %s", __func__, str);
    return str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return (out->config.period_count * out->config.period_size * 1000) /
           (out->config.rate);
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right __unused)
{
    struct stream_out *out = (struct stream_out *)stream;

    if (out->usecase == USECASE_AUDIO_PLAYBACK_MULTI_CH) {
        /* only take left channel into account: the API is for stereo anyway */
        out->muted = (left == 0.0f);
        return 0;
    }

    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    ssize_t ret = 0;
    struct pcm_device *pcm_device;
    struct listnode *node;

    lock_output_stream(out);
    if (out->standby) {

        pthread_mutex_lock(&adev->lock);
        ret = start_output_stream(out);
        if (ret != 0) {
            pthread_mutex_unlock(&adev->lock);

            goto exit;
        }
        out->standby = false;


        pthread_mutex_unlock(&adev->lock);

    }

    if (out->muted)
        memset((void *)buffer, 0, bytes);
    list_for_each(node, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        if (pcm_device->pcm) {
            size_t src_channels = audio_channel_count_from_out_mask(out->channel_mask);
            size_t dst_channels = pcm_device->pcm_profile->config.channels;
            bool channel_remapping_needed = (dst_channels != src_channels);
            unsigned audio_bytes;
            const void *audio_data;

            ALOGVV("%s: writing buffer (%zd bytes) to pcm device", __func__, bytes);

            audio_data = buffer;
            audio_bytes = bytes;

            /*
             * This can only be S16_LE stereo because of the supported formats,
             * 4 bytes per frame.
             */

            if (channel_remapping_needed) {
                size_t dest_buffer_size = audio_bytes * dst_channels / src_channels;
                size_t new_size;
                size_t bytes_per_sample = audio_bytes_per_sample(stream->common.get_format(&stream->common));

                /* With additional channels, we cannot use original buffer */
                if (out->proc_buf_size < dest_buffer_size) {
                    out->proc_buf_size = dest_buffer_size;
                    out->proc_buf_out = realloc(out->proc_buf_out, dest_buffer_size);
                    ALOG_ASSERT((out->proc_buf_out != NULL),
                                "out_write() failed to reallocate proc_buf_out");
                }
                new_size = adjust_channels(audio_data, src_channels, out->proc_buf_out, dst_channels,
                    bytes_per_sample, audio_bytes);
                ALOG_ASSERT(new_size == dest_buffer_size);
                audio_data = out->proc_buf_out;
                audio_bytes = dest_buffer_size;
            }

            pcm_device->status = pcm_write(pcm_device->pcm, audio_data, audio_bytes);
            if (pcm_device->status != 0)
                ret = pcm_device->status;
        }
        if (ret == 0)
            out->written += bytes / (out->config.channels * sizeof(short));
    }

exit:
    pthread_mutex_unlock(&out->lock);

    if (ret != 0) {
        list_for_each(node, &out->pcm_dev_list) {
            pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
            if (pcm_device->pcm && pcm_device->status != 0)
                ALOGE("%s: error %zd - %s", __func__, ret, pcm_get_error(pcm_device->pcm));
        }
        out_standby(&out->stream.common);
        struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };
        clock_gettime(CLOCK_MONOTONIC, &t);
        const int64_t now = (t.tv_sec * 1000000000LL + t.tv_nsec) / 1000;
        const int64_t elapsed_time_since_last_write = now - out->last_write_time_us;
        int64_t sleep_time = bytes * 1000000LL / audio_stream_out_frame_size(stream) /
                   out_get_sample_rate(&stream->common) - elapsed_time_since_last_write;
        if (sleep_time > 0) {
            usleep(sleep_time);
        } else {
            // we don't sleep when we exit standby (this is typical for a real alsa buffer).
            sleep_time = 0;
        }
        out->last_write_time_us = now + sleep_time;
        // last_write_time_us is an approximation of when the (simulated) alsa
        // buffer is believed completely full. The usleep above waits for more space
        // in the buffer, but by the end of the sleep the buffer is considered
        // topped-off.
        //
        // On the subsequent out_write(), we measure the elapsed time spent in
        // the mixer. This is subtracted from the sleep estimate based on frames,
        // thereby accounting for drain in the alsa buffer during mixing.
        // This is a crude approximation; we don't handle underruns precisely.
    }

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream __unused,
                                   uint32_t *dsp_frames)
{
    *dsp_frames = 0;
    return -ENODATA;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    (void)stream;
    (void)timestamp;
    return -ENOSYS;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                   uint64_t *frames, struct timespec *timestamp)
{
    struct stream_out *out = (struct stream_out *)stream;
    int ret = -ENODATA;

    lock_output_stream(out);


        /* FIXME: which device to read from? */
        if (!list_empty(&out->pcm_dev_list)) {
            struct pcm_device *pcm_device;
            struct listnode *node;
            unsigned int avail;

            list_for_each(node, &out->pcm_dev_list) {
                pcm_device = node_to_item(node,
                                          struct pcm_device,
                                          stream_list_node);

                if (pcm_device->pcm != NULL) {
                    if (pcm_get_htimestamp(pcm_device->pcm, &avail, timestamp) == 0) {
                        size_t kernel_buffer_size = out->config.period_size * out->config.period_count;
                        int64_t signed_frames = out->written - kernel_buffer_size + avail;
                        /* This adjustment accounts for buffering after app processor.
                           It is based on estimated DSP latency per use case, rather than exact. */
                        signed_frames -=
                            (render_latency(out->usecase) * out->sample_rate / 1000000LL);

                        /* It would be unusual for this value to be negative, but check just in case ... */
                        if (signed_frames >= 0) {
                            *frames = signed_frames;
                            ret = 0;
                            goto done;
                        }
                    }
                }
            }
    }

done:
    pthread_mutex_unlock(&out->lock);

    return ret;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->requested_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    (void)stream;
    (void)rate;
    return -ENOSYS;
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->main_channels;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    (void)stream;
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    (void)stream;
    (void)format;

    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return get_input_buffer_size(in->requested_rate,
                                 in_get_format(stream),
                                 audio_channel_count_from_in_mask(in->main_channels),
                                 in->usecase_type,
                                 in->devices);
}

static int in_close_pcm_devices(struct stream_in *in)
{
    struct pcm_device *pcm_device;
    struct listnode *node;

    list_for_each(node, &in->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        if (pcm_device) {
            if (pcm_device->pcm)
                pcm_close(pcm_device->pcm);
            pcm_device->pcm = NULL;
        }
    }
    return 0;
}


/* must be called with stream and hw device mutex locked */
static int do_in_standby_l(struct stream_in *in)
{
    int status = 0;

    if (!in->standby) {

        in_close_pcm_devices(in);

        status = stop_input_stream(in);

        if (in->read_buf) {
            free(in->read_buf);
            in->read_buf = NULL;
        }

        in->standby = 1;
    }

    in->last_read_time_us = 0;

    return 0;
}

// called with adev->lock_inputs locked
static int in_standby_l(struct stream_in *in)
{
    struct audio_device *adev = in->dev;
    int status = 0;
    lock_input_stream(in);
    if (!in->standby) {
        pthread_mutex_lock(&adev->lock);
        status = do_in_standby_l(in);
        pthread_mutex_unlock(&adev->lock);
    }
    pthread_mutex_unlock(&in->lock);
    return status;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    int status;
    ALOGV("%s: enter", __func__);
    pthread_mutex_lock(&adev->lock_inputs);
    status = in_standby_l(in);
    pthread_mutex_unlock(&adev->lock_inputs);
    ALOGV("%s: exit:  status(%d)", __func__, status);
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;

    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    struct str_parms *parms;
    char value[32];
    int ret, val = 0;
    struct audio_usecase *uc_info;
    bool do_standby = false;
    struct pcm_device *pcm_device;

    ALOGV("%s: enter: kvpairs=%s", __func__, kvpairs);
    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE, value, sizeof(value));

    pthread_mutex_lock(&adev->lock_inputs);
    lock_input_stream(in);
    pthread_mutex_lock(&adev->lock);
    if (ret >= 0) {
        val = atoi(value);
        /* no audio source uses val == 0 */
        if (((int)in->source != val) && (val != 0)) {
            in->source = val;
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        if (((int)in->devices != val) && (val != 0)) {
            in->devices = val;
            /* If recording is in progress, change the tx device to new device */
            if (!in->standby) {
                uc_info = get_usecase_from_id(adev, in->usecase);
                if (uc_info == NULL) {
                    ALOGE("%s: Could not find the usecase (%d) in the list",
                          __func__, in->usecase);
                } else {
                    if (list_empty(&in->pcm_dev_list))
                        ALOGE("%s: pcm device list empty", __func__);
                    else {
                        pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                                                  struct pcm_device, stream_list_node);
                        if ((pcm_device->pcm_profile->devices & val & ~AUDIO_DEVICE_BIT_IN) == 0) {
                            do_standby = true;
                        }
                    }
                }
                if (do_standby) {
                    ret = do_in_standby_l(in);
                } else
                    ret = select_devices(adev, in->usecase);
            }
        }
    }
    pthread_mutex_unlock(&adev->lock);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&adev->lock_inputs);

    str_parms_destroy(parms);
    return 0;
}

static char* in_get_parameters(const struct audio_stream *stream,
                               const char *keys)
{
    (void)stream;
    (void)keys;

    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    (void)stream;
    (void)gain;

    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer,
                       size_t bytes)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    ssize_t frames = -1;
    int ret = -1;
    int read_and_process_successful = false;

    size_t frames_rq = bytes / audio_stream_in_frame_size(stream);

    /* no need to acquire adev->lock_inputs because API contract prevents a close */
    lock_input_stream(in);
    if (in->standby) {
        pthread_mutex_unlock(&in->lock);
        pthread_mutex_lock(&adev->lock_inputs);
        lock_input_stream(in);
        if (!in->standby) {
            pthread_mutex_unlock(&adev->lock_inputs);
            goto false_alarm;
        }
        pthread_mutex_lock(&adev->lock);
        ret = start_input_stream(in);
        pthread_mutex_unlock(&adev->lock);
        pthread_mutex_unlock(&adev->lock_inputs);
        if (ret != 0) {
            goto exit;
        }
        in->standby = 0;
    }
false_alarm:

    if (!list_empty(&in->pcm_dev_list)) {
        /*
         * Read PCM and:
         * - resample if needed
         * - process if pre-processors are attached
         * - discard unwanted channels
         */
        frames = read_and_process_frames(stream, buffer, frames_rq);
        if (frames >= 0)
            read_and_process_successful = true;
    }

    /*
     * Instead of writing zeroes here, we could trust the hardware
     * to always provide zeroes when muted.
     */
    if (read_and_process_successful == true && adev->mic_mute)
        memset(buffer, 0, bytes);

exit:
    pthread_mutex_unlock(&in->lock);

    if (read_and_process_successful == false) {
        in_standby(&in->stream.common);
        ALOGV("%s: read failed - sleeping for buffer duration", __func__);
        struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };
        clock_gettime(CLOCK_MONOTONIC, &t);
        const int64_t now = (t.tv_sec * 1000000000LL + t.tv_nsec) / 1000;

        // we do a full sleep when exiting standby.
        const bool standby = in->last_read_time_us == 0;
        const int64_t elapsed_time_since_last_read = standby ?
                0 : now - in->last_read_time_us;
        int64_t sleep_time = bytes * 1000000LL / audio_stream_in_frame_size(stream) /
                in_get_sample_rate(&stream->common) - elapsed_time_since_last_read;
        if (sleep_time > 0) {
            usleep(sleep_time);
        } else {
            sleep_time = 0;
        }
        in->last_read_time_us = now + sleep_time;
        // last_read_time_us is an approximation of when the (simulated) alsa
        // buffer is drained by the read, and is empty.
        //
        // On the subsequent in_read(), we measure the elapsed time spent in
        // the recording thread. This is subtracted from the sleep estimate based on frames,
        // thereby accounting for fill in the alsa buffer during the interim.
        memset(buffer, 0, bytes);
    }

    if (bytes > 0) {
        in->frames_read += bytes / audio_stream_in_frame_size(stream);
    }

    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    (void)stream;

    return 0;
}

static int in_get_capture_position(const struct audio_stream_in *stream,
                                   int64_t *frames, int64_t *time)
{
    if (stream == NULL || frames == NULL || time == NULL) {
        return -EINVAL;
    }

    struct stream_in *in = (struct stream_in *)stream;
    struct pcm_device *pcm_device;
    int ret = -ENOSYS;

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    pthread_mutex_lock(&in->lock);
    if (pcm_device->pcm) {
        struct timespec timestamp;
        unsigned int avail;
        if (pcm_get_htimestamp(pcm_device->pcm, &avail, &timestamp) == 0) {
            *frames = in->frames_read + avail;
            *time = timestamp.tv_sec * 1000000000LL + timestamp.tv_nsec;
            ret = 0;
        }
    }

    pthread_mutex_unlock(&in->lock);
    return ret;
}

static int in_add_audio_effect(const struct audio_stream *stream __unused,
                               effect_handle_t effect __unused)
{
    ALOGV("%s: effect %p", __func__, effect);
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream __unused,
                                  effect_handle_t effect __unused)
{
    ALOGV("%s: effect %p", __func__, effect);
    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address __unused)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_out *out;
    int ret = 0;
    struct pcm_device_profile *pcm_profile;

    ALOGV("%s: enter: sample_rate(%d) channel_mask(%#x) devices(%#x) flags(%#x)",
          __func__, config->sample_rate, config->channel_mask, devices, flags);
    *stream_out = NULL;
    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (out == NULL) {
        ret = -ENOMEM;
        goto error_config;
    }

    if (devices == AUDIO_DEVICE_NONE)
        devices = AUDIO_DEVICE_OUT_SPEAKER;

    out->flags = flags;
    out->devices = devices;
    out->dev = adev;
    out->format = config->format;
    out->sample_rate = config->sample_rate;
    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_STEREO;
    out->handle = handle;

    pcm_profile = get_pcm_device(PCM_PLAYBACK, devices);
    if (pcm_profile == NULL) {
        ret = -EINVAL;
        goto error_open;
    }
    out->config = pcm_profile->config;

    /* Init use case and pcm_config */
    if (out->flags & (AUDIO_OUTPUT_FLAG_DEEP_BUFFER)) {
        out->usecase = USECASE_AUDIO_PLAYBACK_DEEP_BUFFER;
        out->config = pcm_device_deep_buffer.config;
        out->sample_rate = out->config.rate;
        ALOGV("%s: use AUDIO_PLAYBACK_DEEP_BUFFER",__func__);
    } else {
        out->usecase = USECASE_AUDIO_PLAYBACK;
        out->sample_rate = out->config.rate;
    }

    if (flags & AUDIO_OUTPUT_FLAG_PRIMARY) {
        if (adev->primary_output == NULL)
            adev->primary_output = out;
        else {
            ALOGE("%s: Primary output is already opened", __func__);
            ret = -EEXIST;
            goto error_open;
        }
    }

    /* Check if this usecase is already existing */
    pthread_mutex_lock(&adev->lock);
    if (get_usecase_from_id(adev, out->usecase) != NULL) {
        ALOGE("%s: Usecase (%d) is already present", __func__, out->usecase);
        pthread_mutex_unlock(&adev->lock);
        ret = -EEXIST;
        goto error_open;
    }
    pthread_mutex_unlock(&adev->lock);

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->stream.get_presentation_position = out_get_presentation_position;

    out->standby = 1;
    /* out->muted = false; by calloc() */
    /* out->written = 0; by calloc() */

    pthread_mutex_init(&out->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&out->pre_lock, (const pthread_mutexattr_t *) NULL);
    pthread_cond_init(&out->cond, (const pthread_condattr_t *) NULL);

    config->format = out->stream.common.get_format(&out->stream.common);
    config->channel_mask = out->stream.common.get_channels(&out->stream.common);
    config->sample_rate = out->stream.common.get_sample_rate(&out->stream.common);

    *stream_out = &out->stream;
    return 0;

error_open:
    free(out);
    *stream_out = NULL;
error_config:
    ALOGV("%s: exit: ret %d", __func__, ret);
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    (void)dev;

    ALOGV("%s: enter", __func__);
    out_standby(&stream->common);
    pthread_cond_destroy(&out->cond);
    pthread_mutex_destroy(&out->lock);
    pthread_mutex_destroy(&out->pre_lock);
    free(out->proc_buf_out);
    free(stream);
    ALOGV("%s: exit", __func__);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms;
    char value[32];
    int ret;

    ALOGV("%s: enter: %s", __func__, kvpairs);

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_BT_NREC, value, sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->bluetooth_nrec = true;
        else
            adev->bluetooth_nrec = false;
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_BT_SCO_WB, value, sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->bluetooth_wb = true;
        else
            adev->bluetooth_wb = false;
    }

    str_parms_destroy(parms);

    return 0;
}

static char* adev_get_parameters(const struct audio_hw_device *dev,
                                 const char *keys)
{
    (void)dev;
    (void)keys;

    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    (void)dev;

    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    int ret = 0;
    struct audio_device *adev = (struct audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    /* cache volume */
    adev->volume = volume;
    ret = set_voice_volume(adev, adev->volume);
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    (void)dev;
    (void)volume;

    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev,
                                  float *volume)
{
    (void)dev;
    (void)volume;

    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    (void)dev;
    (void)muted;

    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    (void)dev;
    (void)muted;

    return -ENOSYS;
}

static void audio_set_pcm_clock_callback(void *data, int clock_state)
{
    struct audio_device *adev = (struct audio_device *)data;

    if (clock_state  && adev->mode == AUDIO_MODE_IN_CALL && !adev->in_call) {
            //ALOGV("starting voice call: %d", clock_state);
            //start_voice_call(adev);
    }
}

static void audio_set_wb_amr_callback(void *data, int wb_amr)
{
    struct audio_device *adev = (struct audio_device *)data;
 
    if (wb_amr && !adev->audience_enabled && !adev->forced_voice_config) {
        if (wb_amr && !adev->wb_amr_type) {
            adev->wb_amr_type = 1;
        } else if (!wb_amr && adev->wb_amr_type) {
            adev->wb_amr_type = 0;
        }
    }

}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    struct audio_device *adev = (struct audio_device *)dev;

    pthread_mutex_lock(&adev->lock);
    if (adev->mode != mode) {
        ALOGI("%s mode = %d", __func__, mode);
        if (mode == AUDIO_MODE_IN_CALL) {
            ril_connect_if_required(&adev->ril);
            ril_set_pcm_clock_callback(&adev->ril,
                                  audio_set_pcm_clock_callback,
                                  (void *)adev);
            ril_set_wb_amr_callback(&adev->ril,
                                  audio_set_wb_amr_callback,
                                  (void *)adev);
        }
        adev->mode = mode;
    }

    if ((adev->mode == AUDIO_MODE_NORMAL) && adev->in_call) {
        ril_set_call_clock_sync(&adev->ril, SOUND_CLOCK_STOP);
        stop_voice_call(adev);
    }

    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct audio_device *adev = (struct audio_device *)dev;
    int err = 0;
    enum _MuteCondition mute_condition = state ? TX_MUTE : TX_UNMUTE;

    pthread_mutex_lock(&adev->lock);
    adev->mic_mute = state;

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        err = ril_set_mute(&adev->ril, mute_condition);
        ALOGE_IF(err != 0, "Failed to set mute: (%d)", err);
    }

    pthread_mutex_unlock(&adev->lock);
    return err;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    *state = adev->mic_mute;

    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    (void)dev;

    /* NOTE: we default to built in mic which may cause a mismatch between what we
     * report here and the actual buffer size
     */
    return get_input_buffer_size(config->sample_rate,
                                 config->format,
                                 audio_channel_count_from_in_mask(config->channel_mask),
                                 PCM_CAPTURE /* usecase_type */,
                                 AUDIO_DEVICE_IN_BUILTIN_MIC);
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle __unused,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags,
                                  const char *address __unused,
                                  audio_source_t source)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in;
    struct pcm_device_profile *pcm_profile;

    ALOGV("%s: enter", __func__);

    *stream_in = NULL;
    if (check_input_parameters(config->sample_rate, config->format,
                               audio_channel_count_from_in_mask(config->channel_mask)) != 0)
        return -EINVAL;

    usecase_type_t usecase_type = PCM_CAPTURE;
    pcm_profile = get_pcm_device(usecase_type, devices);
    if (pcm_profile == NULL)
        return -EINVAL;

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (in == NULL) {
        return -ENOMEM;
    }

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->stream.get_capture_position = in_get_capture_position;

    in->devices = devices;
    in->source = source;
    in->dev = adev;
    in->standby = 1;
    in->main_channels = config->channel_mask;
    in->requested_rate = config->sample_rate;
    if (config->sample_rate != CAPTURE_DEFAULT_SAMPLING_RATE)
        flags = flags & ~AUDIO_INPUT_FLAG_FAST;
    in->input_flags = flags;
    /* HW codec is limited to default channels. No need to update with
     * requested channels */
    in->config = pcm_profile->config;

    /* Update config params with the requested sample rate and channels */
    in->usecase = USECASE_AUDIO_CAPTURE;
    in->usecase_type = usecase_type;

    pthread_mutex_init(&in->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&in->pre_lock, (const pthread_mutexattr_t *) NULL);

    *stream_in = &in->stream;
    ALOGV("%s: exit", __func__);
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                    struct audio_stream_in *stream)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in = (struct stream_in*)stream;
    ALOGV("%s", __func__);

    /* prevent concurrent out_set_parameters, or out_write from standby */
    pthread_mutex_lock(&adev->lock_inputs);

    in_standby_l(in);
    pthread_mutex_destroy(&in->lock);
    pthread_mutex_destroy(&in->pre_lock);
    free(in->proc_buf_out);

    free(stream);

    pthread_mutex_unlock(&adev->lock_inputs);

    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    (void)device;
    (void)fd;

    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;
    audio_device_ref_count--;
    free(adev->snd_dev_ref_cnt);
    free_mixer_list(adev);
    free(device);

    adev = NULL;

    return 0;
}

static int adev_open(const hw_module_t *module, const char *name,
                     hw_device_t **device)
{
    struct audio_device *adev;
    char voice_config[PROPERTY_VALUE_MAX];

    int ret;

    ALOGV("%s: enter", __func__);

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) {
        ALOGV("%s: audio hardware interface is not zero", __func__);
        return -EINVAL;
    }

    *device = NULL;
    
    adev = calloc(1, sizeof(struct audio_device));
    if (adev == NULL) {
        ALOGV("%s: adev is null", __func__);
        return -ENOMEM;
    }

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *)module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.get_master_volume = adev_get_master_volume;
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    /* Set the default route before the PCM stream is opened */
    adev->mode = AUDIO_MODE_NORMAL;
    adev->active_input = NULL;
    adev->primary_output = NULL;
    adev->ns_in_voice_rec = false;
    adev->volume = 1.0f;
    adev->mic_mute = false;
    adev->bluetooth_wb = false;
    adev->bluetooth_nrec = false;
    adev->in_call = false;
    adev->in_call_sco = false;
    adev->in_comm_sco = false;
    adev->wb_amr_type = 0;
    adev->forced_voice_config = 0;

    adev->snd_dev_ref_cnt = calloc(SND_DEVICE_MAX, sizeof(int));
    if (adev->snd_dev_ref_cnt == NULL) {
        free(adev);
        ALOGV("%s: snd_dev_ref_cnt is null", __func__);
        return -ENOMEM;
    }

    list_init(&adev->usecase_list);

    if (mixer_init(adev) != 0) {
        free(adev->snd_dev_ref_cnt);
        free(adev);
        ALOGE("%s: Failed to init, aborting.", __func__);
        *device = NULL;
        return -EINVAL;
    }

   ret = ril_open(&adev->ril);
    if (ret != 0) {
        ALOGV("%s: Bailout no ril open", __func__);
        free(adev);
        return -EINVAL;
    }

    if(property_get_bool("audio_hal.uses.audience", false)) {
        adev->audience_enabled = true;
        ALOGV("%s: audience enabled", __func__);
    }


    ret = property_get("audio_hal.force_voice_config", voice_config, "");
    if (adev->audience_enabled) {
        adev->wb_amr_type = 0;
    } else if (ret > 0) {
        adev->forced_voice_config = 1;
        if ((strncmp(voice_config, "narrow", 6)) == 0)
            adev->wb_amr_type = 0;
        else if ((strncmp(voice_config, "wide", 4)) == 0)
            adev->wb_amr_type = 1;
        ALOGV("%s: Forcing voice config: %s", __func__, voice_config);
    }

    *device = &adev->device.common;

    audio_device_ref_count++;

    ALOGV("%s: exit", __func__);
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Samsung Audio HAL",
        .author = "The LineageOS Project",
        .methods = &hal_module_methods,
    },
};
