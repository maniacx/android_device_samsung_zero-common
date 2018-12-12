/*
 * Copyright (C) 2018 The LineageOS Project
 * Copyright (C) 2018 TeamNexus
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

package org.lineageos.hardware;

import android.util.Log;
import org.lineageos.internal.util.FileUtils;

/**
 * Reader mode
 */
public class ReadingEnhancement {

    private static final String TAG = "ReadingEnhancement";

    private static final String FILE_READING = "/sys/class/mdnie/mdnie/accessibility";

    private static final int MODE_UNSUPPORTED          = 0;
    private static final int MODE_HWC2_COLOR_TRANSFORM = 1;
    private static final int MODE_SYSFS_READING        = 2;

    private static final int sMode;

    private static boolean sEnabled;

    static {
        // Determine mode of operation.
        if (FileUtils.isFileReadable(FILE_READING) &&
                FileUtils.isFileWritable(FILE_READING)) {
            sMode = MODE_SYSFS_READING;
        } else {
            sMode = MODE_UNSUPPORTED;
             Log.e(TAG, "Reading mode not supported");
        }
    }

    /**
     * Whether device supports Reader Mode
     *
     * @return boolean Supported devices must return always true
     */
    public static boolean isSupported() {
        return sMode != MODE_UNSUPPORTED;
    }

    /**
     * This method return the current activation status of Reader Mode
     *
     * @return boolean Must be false when Reader Mode is not supported or not activated,
     * or the operation failed while reading the status; true in any other case.
     */
    public static boolean isEnabled() {
        if (sMode == MODE_SYSFS_READING) {
            try {
                return Integer.parseInt(FileUtils.readOneLine(FILE_READING)) > 0;
            } catch (Exception e) {
                Log.e(TAG, e.getMessage(), e);
            }
        }

        return sEnabled;
    }

    /**
     * This method allows to setup Reader Mode
     *
     * @param status The new Reader Mode status
     * @return boolean Must be false if Reader Mode is not supported or the operation
     * failed; true in any other case.
     */
    public static boolean setEnabled(boolean status) {
        return FileUtils.writeLine(FILE_READING,
            (status ? "4" : "0"));
    }

}
