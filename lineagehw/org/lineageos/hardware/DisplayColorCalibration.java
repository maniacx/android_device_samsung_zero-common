/*
 * Copyright (C) 2017 The LineageOS Project
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

import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Slog;

import org.lineageos.internal.util.FileUtils;

public class DisplayColorCalibration {

    private static final String TAG = "DisplayColorCalibration";
    private static final String RGB_FILE = "/sys/class/mdnie/mdnie/sensorRGB";
    private static final int MIN = 1;
    private static final int MAX = 255;
    private static final int[] sCurColors = new int[] { MAX, MAX, MAX };
    private static final String sDefColors = "255 255 255";

    public static boolean isSupported() {
        return true;
    }

    public static int getMaxValue()  {
        return MAX;
    }

    public static int getMinValue()  {
        return MIN;
    }

    public static int getDefValue() {
        return MAX;
    }

    public static String getCurColors()  {
        return FileUtils.readOneLine(RGB_FILE);
    }

    public static boolean setColors(String colors) {
        return FileUtils.writeLine(RGB_FILE, colors);
    }
}
