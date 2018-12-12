/*
 * Copyright (C) 2013 Xiao-Long Chen <chenxiaolong@cxl.epac.to>
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

import org.lineageos.internal.util.FileUtils;

/**
 * Fade led notification
 */
public class FadeLedNotification {

    private static final String FILE_LEDFADE = "/sys/class/sec/led/led_fade";

    /**
     * Whether device supports fade led notification.
     *
     * @return boolean Supported devices must return always true
     */
    public static boolean isSupported() {
        return FileUtils.isFileWritable(FILE_LEDFADE) &&
                FileUtils.isFileReadable(FILE_LEDFADE);
    }

    /**
     * This method return the current activation status of fade led notification
     *
     * @return boolean Must be false if fade led notification is not supported or not activated,
     * or the operation failed while reading the status; true in any other case.
     */
    public static boolean isEnabled() {
        return "1".equals(FileUtils.readOneLine(FILE_LEDFADE));
    }

    /**
     * This method allows to setup fade led notification status.
     *
     * @param status The new fade led notification status
     * @return boolean Must be false if fade led notificationis not supported or the operation
     * failed; true in any other case.
     */
    public static boolean setEnabled(boolean status) {
        return FileUtils.writeLine(FILE_LEDFADE, status ? "1" : "0");
    }

}
