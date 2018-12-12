/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (C) 2014 The CyanogenMod Project
 * Copyright (C) 2014-2015 Andreas Schneider <asn@cryptomilk.org>
 * Copyright (C) 2014-2017 Christopher N. Hesse <raymanfx@gmail.com>
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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#define LOG_TAG "SamsungPowerHAL"
#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <hardware/power.h>
#include <liblights/samsung_lights_helper.h>

#include "samsung_power.h"

#define BOOST_PATH             CPU0_INTERACTIVE_PATH "/boost"
#define BOOST2_PATH            CPU0_INTERACTIVE_PATH "/boost2"
#define BOOSTPULSE_PATH        CPU0_INTERACTIVE_PATH "/boostpulse"
#define BOOSTPULSE2_PATH       CPU0_INTERACTIVE_PATH "/boostpulse2"
#define BOOSTPULSE2_DUR_PATH   CPU0_INTERACTIVE_PATH "/boostpulse2_duration"
#define CPU0_IO_IS_BUSY_PATH   CPU0_INTERACTIVE_PATH "/io_is_busy"
#define CPU4_IO_IS_BUSY_PATH   CPU4_INTERACTIVE_PATH "/io_is_busy"
#define CPU0_MAX_FREQ_PATH     CPU0_SYSFS_PATH "/cpufreq/scaling_max_freq"
#define CPU4_MAX_FREQ_PATH     CPU4_SYSFS_PATH "/cpufreq/scaling_max_freq"

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct samsung_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse2_fd;
    int boost2_fd;
    char cpu0_max_freq[10];
    char cpu4_max_freq[10];
    char* touchscreen_power_path;
    char* touchkey_power_path;
    bool low_power_mode;
};

enum power_profile_e {
    PROFILE_POWER_SAVE = 0,
    PROFILE_BALANCED
};

static enum power_profile_e current_power_profile = PROFILE_BALANCED;

/**********************************************************
 *** HELPER FUNCTIONS
 **********************************************************/

static int sysfs_read(char *path, char *s, int num_bytes)
{
    char errno_str[64];
    int len;
    int ret = 0;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
//        ALOGE("Error opening %s: %s", path, errno_str);

        return -1;
    }

    len = read(fd, s, num_bytes - 1);
    if (len < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
//        ALOGE("Error reading from %s: %s", path, errno_str);

        ret = -1;
    } else {
        // do not store newlines, but terminate the string instead
        if (s[len-1] == '\n') {
            s[len-1] = '\0';
        } else {
            s[len] = '\0';
        }
    }

    close(fd);

    return ret;
}

static void sysfs_write(const char *path, char *s)
{
    char errno_str[64];
    int len;
    int fd;

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
//        ALOGE("Error opening %s: %s", path, errno_str);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
//        ALOGE("Error writing to %s: %s", path, errno_str);
    }

    close(fd);
}

static void boostpulse_open(struct samsung_power_module *samsung_pwr)
{
    samsung_pwr->boostpulse_fd = open(BOOSTPULSE_PATH, O_WRONLY);
    if (samsung_pwr->boostpulse_fd < 0) {
        ALOGE("Error opening %s: %s\n", BOOSTPULSE_PATH, strerror(errno));
    }
}

static void boostpulse2_open(struct samsung_power_module *samsung_pwr)
{
    char user[PROPERTY_VALUE_MAX];
    char host[PROPERTY_VALUE_MAX];

    property_get("ro.build.user", user, "more");
    property_get("ro.build.host", host, "more");
    if  ((!strcmp(user,"mac")) && (!strcmp(host,"macs18max"))) {
    samsung_pwr->boostpulse2_fd = open(BOOSTPULSE2_PATH, O_WRONLY);
    }else{
    samsung_pwr->boostpulse2_fd = -1;
    }
}

static void boost2_open(struct samsung_power_module *samsung_pwr)
{
    char user[PROPERTY_VALUE_MAX];
    char host[PROPERTY_VALUE_MAX];

    property_get("ro.build.user", user, "more");
    property_get("ro.build.host", host, "more");
    if  ((!strcmp(user,"mac")) && (!strcmp(host,"macs18max"))) {
    samsung_pwr->boost2_fd = open(BOOST2_PATH, O_WRONLY);
    if (samsung_pwr->boost2_fd < 0) {
        ALOGE("Error opening %s: %s\n", BOOST2_PATH, strerror(errno));
    }
    }else{
    samsung_pwr->boost2_fd = -1;
    }
}

static void send_boostpulse(int boostpulse_fd)
{
    if (boostpulse_fd < 0) {
        return;
    }
    write(boostpulse_fd, "1", 1);
}

static void send_boostpulse2(int boostpulse2_fd)
{
    if (boostpulse2_fd < 0) {
        return;
    }
    write(boostpulse2_fd, "1", 1);
}

static void stop_boostpulse2(int boost2_fd)
{
    if (boost2_fd < 0) {
        return;
    }
    write(boost2_fd, "0", 1);
}

/**********************************************************
 *** POWER FUNCTIONS
 **********************************************************/

static void set_power_profile(struct samsung_power_module *samsung_pwr,
                              int profile)
{
    int rc;

    if (current_power_profile == profile) {
        return;
    }

    ALOGV("%s: profile=%d", __func__, profile);

    switch (profile) {
        case PROFILE_POWER_SAVE:
            sysfs_read(CPU0_MAX_FREQ_PATH, samsung_pwr->cpu0_max_freq, sizeof(samsung_pwr->cpu0_max_freq));
            sysfs_read(CPU4_MAX_FREQ_PATH, samsung_pwr->cpu4_max_freq, sizeof(samsung_pwr->cpu4_max_freq));
            sysfs_write(CPU0_MAX_FREQ_PATH, "1296000");
            sysfs_write(CPU4_MAX_FREQ_PATH, "800000");
            ALOGV("%s: set powersave mode", __func__);
            break;
        case PROFILE_BALANCED:
            sysfs_write(CPU0_MAX_FREQ_PATH, samsung_pwr->cpu0_max_freq);
            sysfs_write(CPU4_MAX_FREQ_PATH, samsung_pwr->cpu4_max_freq);
            ALOGV("%s: set balanced mode", __func__);
            break;
    }

    current_power_profile = profile;
}

/**********************************************************
 *** INIT FUNCTIONS
 **********************************************************/

static void init_cpufreqs(struct samsung_power_module *samsung_pwr)
{
    sysfs_read(CPU0_MAX_FREQ_PATH, samsung_pwr->cpu0_max_freq, sizeof(samsung_pwr->cpu0_max_freq));
    sysfs_read(CPU4_MAX_FREQ_PATH, samsung_pwr->cpu4_max_freq, sizeof(samsung_pwr->cpu4_max_freq));

    sysfs_write(BOOSTPULSE2_DUR_PATH, "3000000");
}


static void samsung_power_init(struct power_module *module)
{
    struct samsung_power_module *samsung_pwr = (struct samsung_power_module *) module;

    samsung_pwr->low_power_mode = false;
    init_cpufreqs(samsung_pwr);

    boostpulse_open(samsung_pwr);
    boostpulse2_open(samsung_pwr);
    boost2_open(samsung_pwr);

    samsung_pwr->touchscreen_power_path = "/sys/class/sec/tsp/input/enabled";
    samsung_pwr->touchkey_power_path = "/sys/class/sec/sec_touchkey/input/enabled";
}

/**********************************************************
 *** API FUNCTIONS
 ***
 *** Refer to power.h for documentation.
 **********************************************************/

static void samsung_power_set_interactive(struct power_module *module, int on)
{
    struct samsung_power_module *samsung_pwr = (struct samsung_power_module *) module;
    int panel_brightness;
    char button_state[2];
    int rc;
    static bool touchkeys_blocked = false;

    ALOGV("power_set_interactive: %d", on);

    /*
     * Do not disable any input devices if the screen is on but we are in a non-interactive
     * state.
     */
    if (!on) {
        panel_brightness = get_cur_panel_brightness();
        if (panel_brightness < 0) {
            ALOGE("%s: Failed to read panel brightness", __func__);
        } else if (panel_brightness > 0) {
            ALOGV("%s: Moving to non-interactive state, but screen is still on,"
                  " not disabling input devices", __func__);
            goto out;
        }
    }

    /* Sanity check the touchscreen path */
    if (samsung_pwr->touchscreen_power_path) {
        sysfs_write(samsung_pwr->touchscreen_power_path, on ? "1" : "0");
    }

    /* Bail out if the device does not have touchkeys */
    if (samsung_pwr->touchkey_power_path == NULL) {
        goto out;
    }

    if (!on) {
        rc = sysfs_read(samsung_pwr->touchkey_power_path, button_state, ARRAY_SIZE(button_state));
        if (rc < 0) {
            ALOGE("%s: Failed to read touchkey state", __func__);
            goto out;
        }
        /*
         * If button_state is 0, the keys have been disabled by another component
         * (for example cmhw), which means we don't want them to be enabled when resuming
         * from suspend.
         */
        if (button_state[0] == '0') {
            touchkeys_blocked = true;
        } else {
            touchkeys_blocked = false;
        }
    }

    if (!touchkeys_blocked) {
        sysfs_write(samsung_pwr->touchkey_power_path, on ? "1" : "0");
    }

out:
    sysfs_write(CPU0_IO_IS_BUSY_PATH, on ? "1" : "0");
    sysfs_write(CPU4_IO_IS_BUSY_PATH, on ? "1" : "0");
    ALOGV("power_set_interactive: %d done", on);
}

static void samsung_power_hint(struct power_module *module,
                                  power_hint_t hint,
                                  void *data)
{
    struct samsung_power_module *samsung_pwr = (struct samsung_power_module *) module;

    /* Bail out if low-power mode is active */
    if (current_power_profile == PROFILE_POWER_SAVE && hint != POWER_HINT_LOW_POWER
            && hint != POWER_HINT_DISABLE_TOUCH) {
        ALOGV("%s: PROFILE_POWER_SAVE active, ignoring hint %d", __func__, hint);
        return;
    }

    switch (hint) {
        case POWER_HINT_VSYNC:
            stop_boostpulse2(samsung_pwr->boost2_fd);
            break;
        case POWER_HINT_INTERACTION:
            send_boostpulse2(samsung_pwr->boostpulse2_fd);
            break;
        case POWER_HINT_LOW_POWER:
            set_power_profile(samsung_pwr, data ? PROFILE_POWER_SAVE : PROFILE_BALANCED);
            break;
        case POWER_HINT_LAUNCH:
            send_boostpulse(samsung_pwr->boostpulse_fd);
            break;
        case POWER_HINT_DISABLE_TOUCH:
            sysfs_write(samsung_pwr->touchscreen_power_path, data ? "0" : "1");
            break;
        default:
            ALOGW("%s: Unknown power hint: %d", __func__, hint);
            break;
    }
}

static void samsung_set_feature(struct power_module *module, feature_t feature, int state __unused)
{
    struct samsung_power_module *samsung_pwr = (struct samsung_power_module *) module;

    switch (feature) {
#ifdef TARGET_TAP_TO_WAKE_NODE
        case POWER_FEATURE_DOUBLE_TAP_TO_WAKE:
            if (state) {
            ALOGV("%s: enabling double tap to wake", __func__);
            sysfs_write("/proc/touchscreen/doubletap2wake", "1");
            } else {
            ALOGV("%s: disabling double tap to wake", __func__);
            sysfs_write("/proc/touchscreen/doubletap2wake", "0");
            }
            break;
#endif
        default:
            break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct samsung_power_module HAL_MODULE_INFO_SYM = {
    .base = {
        .common = {
            .tag = HARDWARE_MODULE_TAG,
            .module_api_version = POWER_MODULE_API_VERSION_0_2,
            .hal_api_version = HARDWARE_HAL_API_VERSION,
            .id = POWER_HARDWARE_MODULE_ID,
            .name = "Samsung Power HAL",
            .author = "The LineageOS Project",
            .methods = &power_module_methods,
        },

        .init = samsung_power_init,
        .setInteractive = samsung_power_set_interactive,
        .powerHint = samsung_power_hint,
        .setFeature = samsung_set_feature
    },

    .lock = PTHREAD_MUTEX_INITIALIZER,
    .boostpulse_fd = -1,
};
