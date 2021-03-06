ifeq ($(TARGET_INIT_VENDOR_LIB),libinit_sec)

LOCAL_PATH := $(call my-dir)
LIBINIT_SEC_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := \
    system/core/base/include \
    system/core/init \
    external/selinux/libselinux/include

LOCAL_CFLAGS := -Wall -DANDROID_TARGET=\"$(TARGET_BOARD_PLATFORM)\"
LOCAL_STATIC_LIBRARIES := libbase libselinux
LOCAL_SRC_FILES := init_sec.cpp
LOCAL_MODULE := libinit_sec
include $(BUILD_STATIC_LIBRARY)

endif
