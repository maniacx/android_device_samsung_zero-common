/*
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

#define LOG_TAG "audio_hw_audience"
#define LOG_NDEBUG 0

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cutils/log.h>
#include "audience-routes.h"
#include "audience.h"

/*
 * Writes an Integer to a file.
 *
 * @param path The absolute path string.
 * @param value The Integer value to be written.
 * @return 0 on success, errno on error.
 */
static int write_int(char const *path, const int value)
{
    int fd, len, num_bytes;
    int ret = 0;
    char buffer[20];

    ALOGV("%s: audience test", __func__);

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        ret = errno;
        ALOGE("%s: failed to open %s (%s)", __func__, path, strerror(errno));
        goto exit;
    }

    num_bytes = sprintf(buffer, "%d", value);
    len = write(fd, buffer, num_bytes);
    if (len < 0) {
        ret = errno;
        ALOGE("%s: failed to write to %s (%s)", __func__, path, strerror(errno));
        goto exit;
    }

exit:
    close(fd);
    return ret;
}

/*
 * Writes the route value to sysfs.
 *
 * @param value The long Integer value to be written.
 * @return 0 on success, -1 or errno on error.
 */
static int es_route_value_set(int value)
{
    ALOGV("%s: audience test", __func__);
    return write_int(SYSFS_PATH_PRESET, value);
}

/*
 * Writes the veq control to sysfs.
 *
 * @param value The Integer value to be written.
 * @return 0 on success, -1 or errno on error.
 */
static int es_veq_control_set(int value)
{
    ALOGV("%s: audience test", __func__);
    return write_int(SYSFS_PATH_VEQ, value);
}

/*
 * Writes the extra volume to sysfs.
 *
 * @param value The Integer value to be written.
 * @return 0 on success, -1 or errno on error.
 */
static int es_extra_volume_set(int value)
{
    return write_int(SYSFS_PATH_EXTRAVOLUME, value);
}

/*
 * Convertes an out_device from the session to an earSmart compatible route.
 *
 * @param out_device The output device to be converted.
 * @return Audience earSmart route, coded as long Integer.
 */
static long es_device_to_route(struct audio_device *adev)
{
    long route;

    if (adev->primary_output->devices & AUDIO_DEVICE_OUT_EARPIECE) {
        if (adev->wb_amr_type >= 1) {
            route = Call_CT_WB;
        } else {
            route = Call_CT_NB;
        }
    } else if (adev->primary_output->devices & AUDIO_DEVICE_OUT_SPEAKER) {
        if (adev->wb_amr_type >= 1) {
            route = Call_FT_WB;
        } else {
            route = Call_FT_NB;
        }
    } else if ((adev->primary_output->devices & AUDIO_DEVICE_OUT_WIRED_HEADSET)
		|| (adev->primary_output->devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
        if (adev->wb_amr_type >= 1) {
            route = Call_HS_WB;
        } else {
            route = Call_HS_NB;
        }
    } else if ((adev->primary_output->devices & AUDIO_DEVICE_OUT_ALL_SCO)
		|| (adev->primary_output->devices & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET)
		|| (adev->primary_output->devices & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT)) {
        if (adev->bluetooth_wb >= 1) {
            route = Call_BT_WB;
        } else {
            route = Call_BT_NB;
        }
    } else {
            ALOGE("%s: unknown output device. defaulting to earpiece", __func__);
        if (adev->wb_amr_type >= 1) {
            route = Call_CT_WB;
        } else {
            route = Call_CT_NB;
        }
    }
    ALOGV("%s: audience test", __func__);
    return route;
}

/*
 * Configures and enables the Audience earSmart IC.
 *
 * @param session Reference to the active voice call session.
 * @return @return 0 on success, -1 or errno on error.
 */
int es_start_voice_session(struct audio_device *adev)
{
    int ret;
    long es_route = es_device_to_route(adev);

    /* TODO: Calculate these */
    int extra_volume = 0;
    int veq_control = 4;

    /*
     * The control flow for audience earSmart chip is as follows:
     *
     * route_value >> power_control(internal) >> extra_volume >> veq_control
     */
    ret = es_route_value_set(es_route);
    if (ret != 0) {
        ALOGE("%s: es_route_value_set(%ld) failed with code: %d", __func__, es_route, ret);
        goto exit;
    }
    ret = es_extra_volume_set(extra_volume);
    if (ret != 0) {
        ALOGE("%s: es_extra_volume_set(%d) failed with code: %d", __func__, extra_volume, ret);
        goto exit;
    }
    ret = es_veq_control_set(veq_control);
    if (ret != 0) {
        ALOGE("%s: es_veq_control_set(%d) failed with code: %d", __func__, veq_control, ret);
        goto exit;
    }
    ALOGV("%s: audience test", __func__);
exit:
    return ret;
}

/*
 * Disables the Audience earSmart IC.
 */
void es_stop_voice_session()
{
    ALOGV("%s: audience test", __func__);
    /* This will cancel any pending workers, stop the stream and send the IC to sleep */
    es_route_value_set(AUDIENCE_SLEEP);
}
