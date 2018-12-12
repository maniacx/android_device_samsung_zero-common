/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "CameraParams"
#include <utils/Log.h>

#include <string.h>
#include <stdlib.h>
#include <camera/CameraParameters.h>
#include <system/graphics.h>

namespace android {
// Parameter keys to communicate between camera application and driver.
const char CameraParameters::KEY_PREVIEW_SIZE[] = "preview-size";
const char CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES[] = "preview-size-values";
const char CameraParameters::KEY_PREVIEW_FORMAT[] = "preview-format";
const char CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS[] = "preview-format-values";
const char CameraParameters::KEY_PREVIEW_FRAME_RATE[] = "preview-frame-rate";
const char CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES[] = "preview-frame-rate-values";
const char CameraParameters::KEY_PREVIEW_FPS_RANGE[] = "preview-fps-range";
const char CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE[] = "preview-fps-range-values";
const char CameraParameters::KEY_PICTURE_SIZE[] = "picture-size";
const char CameraParameters::KEY_SUPPORTED_PICTURE_SIZES[] = "picture-size-values";
const char CameraParameters::KEY_PICTURE_FORMAT[] = "picture-format";
const char CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS[] = "picture-format-values";
const char CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH[] = "jpeg-thumbnail-width";
const char CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT[] = "jpeg-thumbnail-height";
const char CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES[] = "jpeg-thumbnail-size-values";
const char CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY[] = "jpeg-thumbnail-quality";
const char CameraParameters::KEY_JPEG_QUALITY[] = "jpeg-quality";
const char CameraParameters::KEY_ROTATION[] = "rotation";
const char CameraParameters::KEY_GPS_LATITUDE[] = "gps-latitude";
const char CameraParameters::KEY_GPS_LONGITUDE[] = "gps-longitude";
const char CameraParameters::KEY_GPS_ALTITUDE[] = "gps-altitude";
const char CameraParameters::KEY_GPS_TIMESTAMP[] = "gps-timestamp";
const char CameraParameters::KEY_GPS_PROCESSING_METHOD[] = "gps-processing-method";
const char CameraParameters::KEY_WHITE_BALANCE[] = "whitebalance";
const char CameraParameters::KEY_SUPPORTED_WHITE_BALANCE[] = "whitebalance-values";
const char CameraParameters::KEY_EFFECT[] = "effect";
const char CameraParameters::KEY_SUPPORTED_EFFECTS[] = "effect-values";
const char CameraParameters::KEY_ANTIBANDING[] = "antibanding";
const char CameraParameters::KEY_SUPPORTED_ANTIBANDING[] = "antibanding-values";
const char CameraParameters::KEY_SCENE_MODE[] = "scene-mode";
const char CameraParameters::KEY_SUPPORTED_SCENE_MODES[] = "scene-mode-values";
const char CameraParameters::KEY_FLASH_MODE[] = "flash-mode";
const char CameraParameters::KEY_SUPPORTED_FLASH_MODES[] = "flash-mode-values";
const char CameraParameters::KEY_FOCUS_MODE[] = "focus-mode";
const char CameraParameters::KEY_SUPPORTED_FOCUS_MODES[] = "focus-mode-values";
const char CameraParameters::KEY_MAX_NUM_FOCUS_AREAS[] = "max-num-focus-areas";
const char CameraParameters::KEY_FOCUS_AREAS[] = "focus-areas";
const char CameraParameters::KEY_FOCAL_LENGTH[] = "focal-length";
const char CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE[] = "horizontal-view-angle";
const char CameraParameters::KEY_VERTICAL_VIEW_ANGLE[] = "vertical-view-angle";
const char CameraParameters::KEY_EXPOSURE_COMPENSATION[] = "exposure-compensation";
const char CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION[] = "max-exposure-compensation";
const char CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION[] = "min-exposure-compensation";
const char CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP[] = "exposure-compensation-step";
const char CameraParameters::KEY_AUTO_EXPOSURE_LOCK[] = "auto-exposure-lock";
const char CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED[] = "auto-exposure-lock-supported";
const char CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK[] = "auto-whitebalance-lock";
const char CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED[] = "auto-whitebalance-lock-supported";
const char CameraParameters::KEY_MAX_NUM_METERING_AREAS[] = "max-num-metering-areas";
const char CameraParameters::KEY_METERING_AREAS[] = "metering-areas";
const char CameraParameters::KEY_ZOOM[] = "zoom";
const char CameraParameters::KEY_MAX_ZOOM[] = "max-zoom";
const char CameraParameters::KEY_ZOOM_RATIOS[] = "zoom-ratios";
const char CameraParameters::KEY_ZOOM_SUPPORTED[] = "zoom-supported";
const char CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED[] = "smooth-zoom-supported";
const char CameraParameters::KEY_FOCUS_DISTANCES[] = "focus-distances";
const char CameraParameters::KEY_VIDEO_FRAME_FORMAT[] = "video-frame-format";
const char CameraParameters::KEY_VIDEO_SIZE[] = "video-size";
const char CameraParameters::KEY_SUPPORTED_VIDEO_SIZES[] = "video-size-values";
const char CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO[] = "preferred-preview-size-for-video";
const char CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW[] = "max-num-detected-faces-hw";
const char CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW[] = "max-num-detected-faces-sw";
const char CameraParameters::KEY_RECORDING_HINT[] = "recording-hint";
const char CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED[] = "video-snapshot-supported";
const char CameraParameters::KEY_VIDEO_STABILIZATION[] = "video-stabilization";
const char CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED[] = "video-stabilization-supported";
const char CameraParameters::KEY_LIGHTFX[] = "light-fx";

const char CameraParameters::TRUE[] = "true";
const char CameraParameters::FALSE[] = "false";
const char CameraParameters::FOCUS_DISTANCE_INFINITY[] = "Infinity";

// Values for white balance settings.
const char CameraParameters::WHITE_BALANCE_AUTO[] = "auto";
const char CameraParameters::WHITE_BALANCE_INCANDESCENT[] = "incandescent";
const char CameraParameters::WHITE_BALANCE_FLUORESCENT[] = "fluorescent";
const char CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT[] = "warm-fluorescent";
const char CameraParameters::WHITE_BALANCE_DAYLIGHT[] = "daylight";
const char CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT[] = "cloudy-daylight";
const char CameraParameters::WHITE_BALANCE_TWILIGHT[] = "twilight";
const char CameraParameters::WHITE_BALANCE_SHADE[] = "shade";

// Values for effect settings.
const char CameraParameters::EFFECT_NONE[] = "none";
const char CameraParameters::EFFECT_MONO[] = "mono";
const char CameraParameters::EFFECT_NEGATIVE[] = "negative";
const char CameraParameters::EFFECT_SOLARIZE[] = "solarize";
const char CameraParameters::EFFECT_SEPIA[] = "sepia";
const char CameraParameters::EFFECT_POSTERIZE[] = "posterize";
const char CameraParameters::EFFECT_WHITEBOARD[] = "whiteboard";
const char CameraParameters::EFFECT_BLACKBOARD[] = "blackboard";
const char CameraParameters::EFFECT_AQUA[] = "aqua";
const char CameraParameters::EFFECT_ANTIQUE[] = "antique"; //Samsung
const char CameraParameters::EFFECT_POINT_BLUE[] = "point-blue"; //Samsung
const char CameraParameters::EFFECT_POINT_RED[] = "point-red"; //Samsung
const char CameraParameters::EFFECT_POINT_YELLOW[] = "point-yellow"; //Samsung
const char CameraParameters::EFFECT_WARM[] = "warm"; //Samsung
const char CameraParameters::EFFECT_COLD[] = "cold"; //Samsung
const char CameraParameters::EFFECT_WASHED[] = "washed"; //Samsung

// Values for antibanding settings.
const char CameraParameters::ANTIBANDING_AUTO[] = "auto";
const char CameraParameters::ANTIBANDING_50HZ[] = "50hz";
const char CameraParameters::ANTIBANDING_60HZ[] = "60hz";
const char CameraParameters::ANTIBANDING_OFF[] = "off";

// Values for flash mode settings.
const char CameraParameters::FLASH_MODE_OFF[] = "off";
const char CameraParameters::FLASH_MODE_AUTO[] = "auto";
const char CameraParameters::FLASH_MODE_ON[] = "on";
const char CameraParameters::FLASH_MODE_RED_EYE[] = "red-eye";
const char CameraParameters::FLASH_MODE_TORCH[] = "torch";
const char CameraParameters::FLASH_MODE_FILLIN[] = "fillin"; //Samsung
const char CameraParameters::FLASH_MODE_SLOW_SYNC[] = "slow"; //Samsung
const char CameraParameters::FLASH_MODE_RED_EYE_FIX[] = "red-eye-fix"; //Samsung
const char CameraParameters::FLASH_VALUE_OF_ISP[] = "flash-value-of-isp"; //Samsung
const char CameraParameters::FLASH_STANDBY_ON[] = "on"; //Samsung
const char CameraParameters::FLASH_STANDBY_OFF[] = "off"; //Samsung

// Values for scene mode settings.
const char CameraParameters::SCENE_MODE_AUTO[] = "auto";
const char CameraParameters::SCENE_MODE_ACTION[] = "action";
const char CameraParameters::SCENE_MODE_PORTRAIT[] = "portrait";
const char CameraParameters::SCENE_MODE_LANDSCAPE[] = "landscape";
const char CameraParameters::SCENE_MODE_NIGHT[] = "night";
const char CameraParameters::SCENE_MODE_NIGHT_PORTRAIT[] = "night-portrait";
const char CameraParameters::SCENE_MODE_THEATRE[] = "theatre";
const char CameraParameters::SCENE_MODE_BEACH[] = "beach";
const char CameraParameters::SCENE_MODE_SNOW[] = "snow";
const char CameraParameters::SCENE_MODE_SUNSET[] = "sunset";
const char CameraParameters::SCENE_MODE_STEADYPHOTO[] = "steadyphoto";
const char CameraParameters::SCENE_MODE_FIREWORKS[] = "fireworks";
const char CameraParameters::SCENE_MODE_SPORTS[] = "sports";
const char CameraParameters::SCENE_MODE_PARTY[] = "party";
const char CameraParameters::SCENE_MODE_CANDLELIGHT[] = "candlelight";
const char CameraParameters::SCENE_MODE_BARCODE[] = "barcode";
const char CameraParameters::SCENE_MODE_HDR[] = "hdr";
const char CameraParameters::SCENE_MODE_BEACH_SNOW[] = "beach-snow"; //Samsung
const char CameraParameters::SCENE_MODE_DUSK_DAWN[] = "dusk-dawn"; //Samsung
const char CameraParameters::SCENE_MODE_FALL_COLOR[] = "fall-color"; //Samsung
const char CameraParameters::SCENE_MODE_BACK_LIGHT[] = "back-light"; //Samsung


const char CameraParameters::PIXEL_FORMAT_YUV422SP[] = "yuv422sp";
const char CameraParameters::PIXEL_FORMAT_YUV420SP[] = "yuv420sp";
const char CameraParameters::PIXEL_FORMAT_YUV422I[] = "yuv422i-yuyv";
const char CameraParameters::PIXEL_FORMAT_YUV420P[]  = "yuv420p";
const char CameraParameters::PIXEL_FORMAT_RGB565[] = "rgb565";
const char CameraParameters::PIXEL_FORMAT_RGBA8888[] = "rgba8888";
const char CameraParameters::PIXEL_FORMAT_JPEG[] = "jpeg";
const char CameraParameters::PIXEL_FORMAT_BAYER_RGGB[] = "bayer-rggb";
const char CameraParameters::PIXEL_FORMAT_ANDROID_OPAQUE[] = "android-opaque";
const char CameraParameters::PIXEL_FORMAT_YUV420SP_NV21[] = "nv21"; //Samsung

// Values for focus mode settings.
const char CameraParameters::FOCUS_MODE_AUTO[] = "auto";
const char CameraParameters::FOCUS_MODE_INFINITY[] = "infinity";
const char CameraParameters::FOCUS_MODE_MACRO[] = "macro";
const char CameraParameters::FOCUS_MODE_FIXED[] = "fixed";
const char CameraParameters::FOCUS_MODE_EDOF[] = "edof";
const char CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO[] = "continuous-video";
const char CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE[] = "continuous-picture";
const char CameraParameters::FOCUS_MODE_MULTI[] = "multi"; //Samsung
const char CameraParameters::FOCUS_MODE_TOUCH[] = "touch"; //Samsung
const char CameraParameters::FOCUS_MODE_OBJECT_TRACKING[] = "object-tracking"; //Samsung
const char CameraParameters::FOCUS_MODE_FACE_DETECTION[] = "face-detection"; //Samsung
const char CameraParameters::FOCUS_MODE_SMART_SELF[] = "self"; //Samsung
const char CameraParameters::FOCUS_MODE_MANUAL[] = "manual"; //Samsung
const char CameraParameters::FOCUS_MODE_FIXED_FACE[] = "fixed-face"; //Samsung

// Values for light fx settings
const char CameraParameters::LIGHTFX_LOWLIGHT[] = "low-light";
const char CameraParameters::LIGHTFX_HDR[] = "high-dynamic-range";

// Samsung Settings
const char CameraParameters::KEY_SUPPORTED_EFFECT_PREVIEW_FPS_RANGE[] = "effect-available-fps-values";
const char CameraParameters::KEY_CONTRAST[] = "contrast";
const char CameraParameters::KEY_SHARPNESS[] = "sharpness";
const char CameraParameters::KEY_SATURATION[] = "saturation";
const char CameraParameters::KEY_MIN_SATURATION[] = "saturation-min";
const char CameraParameters::KEY_MAX_SATURATION[] = "saturation-max";
const char CameraParameters::KEY_SHUTTER_SPEED[] = "shutter-speed";
const char CameraParameters::KEY_APERTURE[] = "aperture";
const char CameraParameters::KEY_AUTO_VALUE[] = "auto-value";
const char CameraParameters::KEY_RAW_SAVE[] = "raw-save";
const char CameraParameters::KEY_CAPTURE_BURST_FILEPATH[] = "capture-burst-filepath";
const char CameraParameters::KEY_CURRENT_ADDRESS[] = "current-address";
const char CameraParameters::KEY_WEATHER[] = "contextualtag-weather";
const char CameraParameters::KEY_CITYID[] = "contextualtag-cityid";

const char CameraParameters::SMART_SCENE_DETECT_OFF[] = "off";
const char CameraParameters::SMART_SCENE_DETECT_ON[] = "on";

// White balance manual
const char CameraParameters::KEY_WHITE_BALANCE_K[] = "wb-k";
const char CameraParameters::WHITE_BALANCE_K[] = "temperature";

// Exposure
const char CameraParameters::KEY_EXPOSURE_TIME[] = "exposure-time";
const char CameraParameters::KEY_MAX_EXPOSURE_TIME[] = "max-exposure-time";
const char CameraParameters::KEY_MIN_EXPOSURE_TIME[] = "min-exposure-time";

// Focus range
const char CameraParameters::KEY_FOCUS_RANGE[] = "focus-range";
const char CameraParameters::FOCUS_RANGE_AUTO[] = "auto";
const char CameraParameters::FOCUS_RANGE_MACRO[] = "macro";
const char CameraParameters::FOCUS_RANGE_AUTO_MACRO[] = "auto-macro";

// Focus area modes
const char CameraParameters::KEY_FOCUS_AREA_MODE[] = "focus-area-mode";
const char CameraParameters::FOCUS_AREA_CENTER[] = "center";
const char CameraParameters::FOCUS_AREA_MULTI[] = "multi";
const char CameraParameters::FOCUS_AREA_SMART_TOUCH[] = "smart-touch";

// Continous mode
const char CameraParameters::KEY_CONTINUOUS_MODE[] = "continuous-mode";
const char CameraParameters::CONTINUOUS_OFF[] = "off";
const char CameraParameters::CONTINUOUS_ON[] = "on";

// ISO
const char CameraParameters::KEY_ISO[] = "iso";
const char CameraParameters::ISO_AUTO[] = "auto";
const char CameraParameters::ISO_50[] = "50";
const char CameraParameters::ISO_80[] = "80";
const char CameraParameters::ISO_100[] = "100";
const char CameraParameters::ISO_200[] = "200";
const char CameraParameters::ISO_400[] = "400";
const char CameraParameters::ISO_800[] = "800";
const char CameraParameters::ISO_1600[] = "1600";
const char CameraParameters::ISO_3200[] = "3200";
const char CameraParameters::ISO_6400[] = "6400";
const char CameraParameters::ISO_SPORTS[] = "sports";
const char CameraParameters::ISO_NIGHT[] = "night";

// DRC
const char CameraParameters::KEY_DYNAMIC_RANGE_CONTROL[] = "dynamic-range-control";
const char CameraParameters::KEY_SUPPORTED_DYNAMIC_RANGE_CONTROL[] = "dynamic-range-control-values";
const char CameraParameters::DRC_OFF[] = "off";
const char CameraParameters::DRC_ON[] = "on";

// Realtime HDR 
const char CameraParameters::KEY_RT_HDR[] = "rt-hdr";
const char CameraParameters::KEY_SUPPORTED_RT_HDR[] = "rt-hdr-values";
const char CameraParameters::RTHDR_OFF[] = "off";
const char CameraParameters::RTHDR_ON[] = "on";
const char CameraParameters::RTHDR_AUTO[] = "auto";

// Phase AF
const char CameraParameters::KEY_PHASE_AF[] = "phase-af";
const char CameraParameters::KEY_SUPPORTED_PHASE_AF[] = "phase-af-values";
const char CameraParameters::PAF_OFF[] = "off";
const char CameraParameters::PAF_ON[] = "on";

// OIS
const char CameraParameters::KEY_OIS[] = "ois";
const char CameraParameters::KEY_OIS_SUPPORTED[] = "ois-supported";
const char CameraParameters::KEY_SUPPORTED_OIS_MODES[] = "ois-mode-values";
const char CameraParameters::OIS_OFF[] = "off";
const char CameraParameters::OIS_ON_STILL[] = "still";
const char CameraParameters::OIS_ON_VIDEO[] = "video";
const char CameraParameters::OIS_ON_ZOOM[] = "zoom";
const char CameraParameters::OIS_ON_SINE_X[] = "sine_x";
const char CameraParameters::OIS_ON_SINE_Y[] = "sine_y";
const char CameraParameters::OIS_CENTERING[] = "center";
const char CameraParameters::OIS_ON_VDIS[] = "vdis";

//Face detection
const char CameraParameters::KEY_FACEDETECT[] = "face-detection";
const char CameraParameters::FACEDETECT_MODE_OFF[] = "off";
const char CameraParameters::FACEDETECT_MODE_NORMAL[] = "normal";
const char CameraParameters::FACEDETECT_MODE_SMILESHOT[] = "smilshot";
const char CameraParameters::FACEDETECT_MODE_BLINK[] = "blink";

// Metering
const char CameraParameters::KEY_METERING[] = "metering";
const char CameraParameters::METERING_OFF[] = "off";
const char CameraParameters::METERING_CENTER[] = "center";
const char CameraParameters::METERING_MATRIX[] = "matrix";
const char CameraParameters::METERING_SPOT[] = "spot";

// Bracket
const char CameraParameters::KEY_BRACKET_AEB[] = "aeb-value";
const char CameraParameters::KEY_BRACKET_WBB[] = "wbb-value";
const char CameraParameters::BRACKET_MODE_OFF[] = "off";
const char CameraParameters::BRACKET_MODE_AEB[] = "aeb";
const char CameraParameters::BRACKET_MODE_WBB[] = "wbb";

// OIS
const char CameraParameters::KEY_IMAGE_STABILIZER[] = "image-stabilizer";
const char CameraParameters::IMAGE_STABILIZER_OFF[] = "off";
const char CameraParameters::IMAGE_STABILIZER_OIS[] = "ois";
const char CameraParameters::IMAGE_STABILIZER_DUALIS[] = "dual-is";

CameraParameters::CameraParameters()
                : mMap()
{
}

CameraParameters::~CameraParameters()
{
}

String8 CameraParameters::flatten() const
{
    String8 flattened("");
    size_t size = mMap.size();

    for (size_t i = 0; i < size; i++) {
        String8 k, v;
        k = mMap.keyAt(i);
        v = mMap.valueAt(i);

        flattened += k;
        flattened += "=";
        flattened += v;
        if (i != size-1)
            flattened += ";";
    }

    return flattened;
}

void CameraParameters::unflatten(const String8 &params)
{
    const char *a = params.string();
    const char *b;

    mMap.clear();

    for (;;) {
        // Find the bounds of the key name.
        b = strchr(a, '=');
        if (b == 0)
            break;

        // Create the key string.
        String8 k(a, (size_t)(b-a));

        // Find the value.
        a = b+1;
        b = strchr(a, ';');
        if (b == 0) {
            // If there's no semicolon, this is the last item.
            String8 v(a);
            mMap.add(k, v);
            break;
        }

        String8 v(a, (size_t)(b-a));
        mMap.add(k, v);
        a = b+1;
    }
}


void CameraParameters::set(const char *key, const char *value)
{
    if (key == NULL || value == NULL)
        return;

    // XXX i think i can do this with strspn()
    if (strchr(key, '=') || strchr(key, ';')) {
        //XXX ALOGE("Key \"%s\"contains invalid character (= or ;)", key);
        return;
    }

    if (strchr(value, '=') || strchr(value, ';')) {
        //XXX ALOGE("Value \"%s\"contains invalid character (= or ;)", value);
        return;
    }

    mMap.replaceValueFor(String8(key), String8(value));
}

void CameraParameters::set(const char *key, int value)
{
    char str[16];
    sprintf(str, "%d", value);
    set(key, str);
}

void CameraParameters::setFloat(const char *key, float value)
{
    char str[16];  // 14 should be enough. We overestimate to be safe.
    snprintf(str, sizeof(str), "%g", value);
    set(key, str);
}

const char *CameraParameters::get(const char *key) const
{
    String8 v = mMap.valueFor(String8(key));
    if (v.length() == 0)
        return 0;
    return v.string();
}

int CameraParameters::getInt(const char *key) const
{
    const char *v = get(key);
    if (v == 0)
        return -1;
    return strtol(v, 0, 0);
}

float CameraParameters::getFloat(const char *key) const
{
    const char *v = get(key);
    if (v == 0) return -1;
    return strtof(v, 0);
}

void CameraParameters::remove(const char *key)
{
    mMap.removeItem(String8(key));
}

// Parse string like "640x480" or "10000,20000"
static int parse_pair(const char *str, int *first, int *second, char delim,
                      char **endptr = NULL)
{
    // Find the first integer.
    char *end;
    int w = (int)strtol(str, &end, 10);
    // If a delimeter does not immediately follow, give up.
    if (*end != delim) {
        ALOGE("Cannot find delimeter (%c) in str=%s", delim, str);
        return -1;
    }

    // Find the second integer, immediately after the delimeter.
    int h = (int)strtol(end+1, &end, 10);

    *first = w;
    *second = h;

    if (endptr) {
        *endptr = end;
    }

    return 0;
}

static void parseSizesList(const char *sizesStr, Vector<Size> &sizes)
{
    if (sizesStr == 0) {
        return;
    }

    char *sizeStartPtr = (char *)sizesStr;

    while (true) {
        int width, height;
        int success = parse_pair(sizeStartPtr, &width, &height, 'x',
                                 &sizeStartPtr);
        if (success == -1 || (*sizeStartPtr != ',' && *sizeStartPtr != '\0')) {
            ALOGE("Picture sizes string \"%s\" contains invalid character.", sizesStr);
            return;
        }
        sizes.push(Size(width, height));

        if (*sizeStartPtr == '\0') {
            return;
        }
        sizeStartPtr++;
    }
}

void CameraParameters::setPreviewSize(int width, int height)
{
    char str[32];
    sprintf(str, "%dx%d", width, height);
    set(KEY_PREVIEW_SIZE, str);
}

void CameraParameters::getPreviewSize(int *width, int *height) const
{
    *width = *height = -1;
    // Get the current string, if it doesn't exist, leave the -1x-1
    const char *p = get(KEY_PREVIEW_SIZE);
    if (p == 0)  return;
    parse_pair(p, width, height, 'x');
}

void CameraParameters::getPreferredPreviewSizeForVideo(int *width, int *height) const
{
    *width = *height = -1;
    const char *p = get(KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO);
    if (p == 0)  return;
    parse_pair(p, width, height, 'x');
}

void CameraParameters::getSupportedPreviewSizes(Vector<Size> &sizes) const
{
    const char *previewSizesStr = get(KEY_SUPPORTED_PREVIEW_SIZES);
    parseSizesList(previewSizesStr, sizes);
}

void CameraParameters::setVideoSize(int width, int height)
{
    char str[32];
    sprintf(str, "%dx%d", width, height);
    set(KEY_VIDEO_SIZE, str);
}

void CameraParameters::getVideoSize(int *width, int *height) const
{
    *width = *height = -1;
    const char *p = get(KEY_VIDEO_SIZE);
    if (p == 0) return;
    parse_pair(p, width, height, 'x');
}

void CameraParameters::getSupportedVideoSizes(Vector<Size> &sizes) const
{
    const char *videoSizesStr = get(KEY_SUPPORTED_VIDEO_SIZES);
    parseSizesList(videoSizesStr, sizes);
}

void CameraParameters::setPreviewFrameRate(int fps)
{
    set(KEY_PREVIEW_FRAME_RATE, fps);
}

int CameraParameters::getPreviewFrameRate() const
{
    return getInt(KEY_PREVIEW_FRAME_RATE);
}

void CameraParameters::getPreviewFpsRange(int *min_fps, int *max_fps) const
{
    *min_fps = *max_fps = -1;
    const char *p = get(KEY_PREVIEW_FPS_RANGE);
    if (p == 0) return;
    parse_pair(p, min_fps, max_fps, ',');
}

void CameraParameters::setPreviewFormat(const char *format)
{
    set(KEY_PREVIEW_FORMAT, format);
}

const char *CameraParameters::getPreviewFormat() const
{
    return get(KEY_PREVIEW_FORMAT);
}

void CameraParameters::setPictureSize(int width, int height)
{
    char str[32];
    sprintf(str, "%dx%d", width, height);
    set(KEY_PICTURE_SIZE, str);
}

void CameraParameters::getPictureSize(int *width, int *height) const
{
    *width = *height = -1;
    // Get the current string, if it doesn't exist, leave the -1x-1
    const char *p = get(KEY_PICTURE_SIZE);
    if (p == 0) return;
    parse_pair(p, width, height, 'x');
}

void CameraParameters::getSupportedPictureSizes(Vector<Size> &sizes) const
{
    const char *pictureSizesStr = get(KEY_SUPPORTED_PICTURE_SIZES);
    parseSizesList(pictureSizesStr, sizes);
}

void CameraParameters::setPictureFormat(const char *format)
{
    set(KEY_PICTURE_FORMAT, format);
}

const char *CameraParameters::getPictureFormat() const
{
    return get(KEY_PICTURE_FORMAT);
}

void CameraParameters::dump() const
{
    ALOGD("dump: mMap.size = %zu", mMap.size());
    for (size_t i = 0; i < mMap.size(); i++) {
        String8 k, v;
        k = mMap.keyAt(i);
        v = mMap.valueAt(i);
        ALOGD("%s: %s\n", k.string(), v.string());
    }
}

status_t CameraParameters::dump(int fd, const Vector<String16>& /*args*/) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    snprintf(buffer, 255, "CameraParameters::dump: mMap.size = %zu\n", mMap.size());
    result.append(buffer);
    for (size_t i = 0; i < mMap.size(); i++) {
        String8 k, v;
        k = mMap.keyAt(i);
        v = mMap.valueAt(i);
        snprintf(buffer, 255, "\t%s: %s\n", k.string(), v.string());
        result.append(buffer);
    }
    write(fd, result.string(), result.size());
    return NO_ERROR;
}

void CameraParameters::getSupportedPreviewFormats(Vector<int>& formats) const {
    const char* supportedPreviewFormats =
          get(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS);

    if (supportedPreviewFormats == NULL) {
        ALOGW("%s: No supported preview formats.", __FUNCTION__);
        return;
    }

    String8 fmtStr(supportedPreviewFormats);
    char* prevFmts = fmtStr.lockBuffer(fmtStr.size());

    char* savePtr;
    char* fmt = strtok_r(prevFmts, ",", &savePtr);
    while (fmt) {
        int actual = previewFormatToEnum(fmt);
        if (actual != -1) {
            formats.add(actual);
        }
        fmt = strtok_r(NULL, ",", &savePtr);
    }
    fmtStr.unlockBuffer(fmtStr.size());
}


int CameraParameters::previewFormatToEnum(const char* format) {
    return
        !format ?
            HAL_PIXEL_FORMAT_YCrCb_420_SP :
        !strcmp(format, PIXEL_FORMAT_YUV422SP) ?
            HAL_PIXEL_FORMAT_YCbCr_422_SP : // NV16
        !strcmp(format, PIXEL_FORMAT_YUV420SP) ?
            HAL_PIXEL_FORMAT_YCrCb_420_SP : // NV21
        !strcmp(format, PIXEL_FORMAT_YUV422I) ?
            HAL_PIXEL_FORMAT_YCbCr_422_I :  // YUY2
        !strcmp(format, PIXEL_FORMAT_YUV420P) ?
            HAL_PIXEL_FORMAT_YV12 :         // YV12
        !strcmp(format, PIXEL_FORMAT_RGB565) ?
            HAL_PIXEL_FORMAT_RGB_565 :      // RGB565
        !strcmp(format, PIXEL_FORMAT_RGBA8888) ?
            HAL_PIXEL_FORMAT_RGBA_8888 :    // RGB8888
        !strcmp(format, PIXEL_FORMAT_BAYER_RGGB) ?
            HAL_PIXEL_FORMAT_RAW16 :   // Raw sensor data
        -1;
}

bool CameraParameters::isEmpty() const {
    return mMap.isEmpty();
}

}; // namespace android
