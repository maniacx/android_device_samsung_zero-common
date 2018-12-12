/*
   Copyright (c) 2016, The CyanogenMod Project. All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android-base/properties.h>

#include "property_service.h"
#include "vendor_init.h"

using android::base::GetProperty;
using android::base::ReadFileToString;

void property_override(char const prop[], char const value[])
{
    prop_info *pi;

    pi = (prop_info*) __system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void property_override_dual(char const system_prop[],
		char const vendor_prop[], char const value[])
{
    property_override(system_prop, value);
    property_override(vendor_prop, value);
}

void is_dual_sim_device()
{
    property_override("ro.multisim.simslotcount", "2");
    property_override("persist.radio.multisim.config", "dsds");
    property_override("rild.libpath2", "/vendor/lib/libsec-ril-dsds.so");
    property_override("ro.telephony.ril.config", "simactivation");
}

void is_cdma_device(bool cellular_type)
{
    if (cellular_type) {
        property_override("persist.radio.snapshot_enabled", "1");
        property_override("persist.radio.snapshot_timer", "22");
        property_override("persist.eons.enabled", "false");
        property_override("persist.rmnet.mux", "enabled");
        property_override("persist.cne.feature", "0");
        property_override("persist.radio.lte_vrte_ltd", "1");
        property_override("persist.security.ams.enforcing", "1");
        property_override("ro.telephony.default_cdma_sub", "1");
        property_override("ril.subscription.types", "NV,RUIM");
        property_override("telephony.lteOnGsmDevice", "0");
        property_override("telephony.lteOnCdmaDevice", "1");
        property_override("persist.gps.qc_nlp_in_use", "1");
        property_override("persist.loc.nlp_name", "com.qualcomm.location");
        property_override("ro.gps.agps_provider", "1");
        property_override("ro.config.combined_signal", "true");
        property_override("ro.config.multimode_cdma", "1");
        property_override("persist.radio.use_se_table_only", "1");
        property_override("ro.telephony.default_network", "10");
    } else {
        property_override("ro.telephony.default_network", "9");
        property_override("telephony.lteOnGsmDevice", "1");
        property_override("telephony.lteOnCdmaDevice", "0");
    }
}

void uses_edge_power_profile(bool status)
{
    std::string s7e_batt = GetProperty("uses.s7e_battery", "");
    if (s7e_batt == "true") {
        property_override("ro.power_profile.override", "power_profile_s6e");
    } else if (status) {
        property_override("ro.power_profile.override", "power_profile_s7e_bat");
    }
}

void vendor_load_properties()
{
    std::string platform;
    std::string bootloader = GetProperty("ro.bootloader", "");
    std::string device;

    platform = GetProperty("ro.board.platform", "");
    if (platform != ANDROID_TARGET)
            return;

    if (bootloader.find("G920FD") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltexx/zeroflte:7.0/NRD90M/G920FXXU5EQL5:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltexx/zeroflte:7.0/NRD90M/G920FXXU5EQL5:user/test-keys");
        property_override("ro.build.description", "zerofltexx-user 7.0 NRD90M G920FXXS5EQG2 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920F");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroflte");
        property_override("ro.product.name", "zerofltexx");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(false);
        is_dual_sim_device();
        is_cdma_device(false);

    } else if (bootloader.find("G920F") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltexx/zeroflte:7.0/NRD90M/G920FXXU5EQL5:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltexx/zeroflte:7.0/NRD90M/G920FXXU5EQL5:user/test-keys");
        property_override("ro.build.description", "zerofltexx-user 7.0 NRD90M G920FXXS5EQG2 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920F");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroflte");
        property_override("ro.product.name", "zerofltexx");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(false);
        is_cdma_device(false);

    } else if (bootloader.find("G920S") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroflteskt/zeroflteskt:7.0/NRD90M/G920SKSU3EQL1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroflteskt/zeroflteskt:7.0/NRD90M/G920SKSU3EQL1:user/test-keys");
        property_override("ro.build.description", "zeroflteskt-user 7.0 NRD90M G920SKSU3EQL1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920S");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroflteskt");
        property_override("ro.product.name", "zeroflteskt");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(false);
        is_cdma_device(false);

    } else if (bootloader.find("G920K") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltektt/zerofltektt:7.0/NRD90M/G920KKKU3EQL1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltektt/zerofltektt:7.0/NRD90M/G920KKKU3EQL1:user/test-keys");
        property_override("ro.build.description", "zerofltektt-user 7.0 NRD90M G920KKKU3EQL1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920K");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerofltektt");
        property_override("ro.product.name", "zerofltektt");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(false);
        is_cdma_device(false);

    } else if (bootloader.find("G920L") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltelgt/zerofltelgt:7.0/NRD90M/G920LKLU3EQL1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltelgt/zerofltelgt:7.0/NRD90M/G920LKLU3EQL1:user/test-keys");
        property_override("ro.build.description", "zerofltelgt-user 7.0 NRD90M G920LKLU3EQL1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920L");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerofltelgt");
        property_override("ro.product.name", "zerofltelgt");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(false);
        is_cdma_device(false);

    } else if (bootloader.find("G920P") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltespr/zerofltespr:7.0/NRD90M/G920PVPS4DQK1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltespr/zerofltespr:7.0/NRD90M/G920PVPS4DQK1:user/test-keys");
        property_override("ro.build.description", "zerofltespr-user 7.0 NRD90M G920PVPS4DQK1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920P");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerofltespr");
        property_override("ro.product.name", "zerofltespr");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(false);
        is_cdma_device(true);

    } else if (bootloader.find("G920I") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltedv/zeroflte:7.0/NRD90M/G920IDVS3FQL3:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltedv/zeroflte:7.0/NRD90M/G920IDVS3FQL3:user/test-keys");
        property_override("ro.build.description", "zerofltedv-user 7.0 NRD90M G920IDVS3FQL3 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920I");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroflte");
        property_override("ro.product.name", "zerofltedv");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-india.so");
        uses_edge_power_profile(false);
        is_cdma_device(false);

    } else if (bootloader.find("G9200") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltezc/zerofltechn:7.0/NRD90M/G9200ZCS2ERC1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltezc/zerofltechn:7.0/NRD90M/G9200ZCS2ERC1:user/test-keys");
        property_override("ro.build.description", "zerofltezc-user 7.0 NRD90M G9200ZCS2ERC1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G9200");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerofltechn");
        property_override("ro.product.name", "zerofltezc");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(false);
        is_dual_sim_device();
        is_cdma_device(true);

    } else if (bootloader.find("G9208") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltezm/zerofltechn:7.0/NRD90M/G9208ZMU2EQK2:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltezm/zerofltechn:7.0/NRD90M/G9208ZMU2EQK2:user/test-keys");
        property_override("ro.build.description", "zerofltezc-user 7.0 NRD90M G9200ZCS2ERC1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G9208");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerofltechn");
        property_override("ro.product.name", "zerofltezm");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(false);
        is_dual_sim_device();
        is_cdma_device(true);

    } else if (bootloader.find("G9209") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltectc/zerofltectc:7.0/NRD90M/G9209KES2ERD1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltectc/zerofltectc:7.0/NRD90M/G9209KES2ERD1:user/test-keys");
        property_override("ro.build.description", "zerofltectc-user 7.0 NRD90M G9209KES2ERD1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G9209");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerofltectc");
        property_override("ro.product.name", "zerofltectc");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(false);
        is_dual_sim_device();
        is_cdma_device(true);

    } else if (bootloader.find("G920R4") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroflteusc/zeroflteusc:7.0/NRD90M/G920R4TYS4DQK2:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroflteusc/zeroflteusc:7.0/NRD90M/G920R4TYS4DQK2:user/test-keys");
        property_override("ro.build.description", "zeroflteusc-user 7.0 NRD90M G920R4TYS4DQK2 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920R4");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroflteusc");
        property_override("ro.product.name", "zeroflteusc");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(false);
        is_dual_sim_device();
        is_cdma_device(true);

    } else if (bootloader.find("G920R7") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroflteacg/zeroflteacg:7.0/NRD90M/G920R7WWS3DQL1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroflteacg/zeroflteacg:7.0/NRD90M/G920R7WWS3DQL1:user/test-keys");
        property_override("ro.build.description", "zeroflteacg-user 7.0 NRD90M G920R7WWS3DQL1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920R7");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroflteacg");
        property_override("ro.product.name", "zeroflteacg");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(false);
        is_dual_sim_device();
        is_cdma_device(true);

    } else if (bootloader.find("G920W8") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltebmc/zerofltebmc:7.0/NRD90M/G920W8VLS5DQK1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltebmc/zerofltebmc:7.0/NRD90M/G920W8VLS5DQK1:user/test-keys");
        property_override("ro.build.description", "zerofltebmc-user 7.0 NRD90M G920W8VLS5DQK1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920W8");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerofltebmc");
        property_override("ro.product.name", "zerofltebmc");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-canada.so");
        uses_edge_power_profile(false);
        is_cdma_device(false);

    } else if (bootloader.find("G920T") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltetmo/zerofltetmo:7.0/NRD90M/G920TUVU5FQK1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltetmo/zerofltetmo:7.0/NRD90M/G920TUVU5FQK1:user/test-keys");
        property_override("ro.build.description", "zerofltetmo-user 7.0 NRD90M G920TUVU5FQK1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920T");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerofltetmo");
        property_override("ro.product.name", "zerofltetmo");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-tmobile.so");
        uses_edge_power_profile(false);
        is_cdma_device(false);

    } else if (bootloader.find("G925F") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltexx/zerolte:7.0/NRD90M/G925FXXU5EQL5:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltexx/zerolte:7.0/NRD90M/G925FXXU5EQL5:user/test-keys");
        property_override("ro.build.description", "zeroltexx-user 7.0 NRD90M G925FXXU5EQL5 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G925F");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltexx");
        property_override("ro.product.name", "zerolte");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(true);
        is_cdma_device(false);

    } else if (bootloader.find("G925S") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerolteskt/zerolteskt:7.0/NRD90M/G925SKSU3EQL1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerolteskt/zerolteskt:7.0/NRD90M/G925SKSU3EQL1:user/test-keys");
        property_override("ro.build.description", "zerolteskt-user 7.0 NRD90M G925SKSU3EQL1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G925S");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerolteskt");
        property_override("ro.product.name", "zerolteskt");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(true);
        is_cdma_device(false);

    } else if (bootloader.find("G925K") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltektt/zeroltektt:7.0/NRD90M/G925KKKU3EQL1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltektt/zeroltektt:7.0/NRD90M/G925KKKU3EQL1:user/test-keys");
        property_override("ro.build.description", "zeroltektt-user 7.0 NRD90M G925KKKU3EQL1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G925K");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltektt");
        property_override("ro.product.name", "zeroltektt");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(true);
        is_cdma_device(false);

    } else if (bootloader.find("G925L") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltelgt/zeroltelgt:7.0/NRD90M/G925LKLU3EQL1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltelgt/zeroltelgt:7.0/NRD90M/G925LKLU3EQL1:user/test-keys");
        property_override("ro.build.description", "zeroltelgt-user 7.0 NRD90M G925LKLU3EQL1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G925L");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltelgt");
        property_override("ro.product.name", "zeroltelgt");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(true);
        is_cdma_device(false);

    } else if (bootloader.find("G925P") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltespr/zeroltespr:7.0/NRD90M/G925PVPS4DQL2:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltespr/zeroltespr:7.0/NRD90M/G925PVPS4DQL2:user/test-keys");
        property_override("ro.build.description", "zeroltespr-user 7.0 NRD90M G925PVPS4DQL2 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920P");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltespr");
        property_override("ro.product.name", "zeroltespr");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(true);
        is_cdma_device(true);

    } else if (bootloader.find("G925I") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltedv/zerolte:7.0/NRD90M/G925IDVU3FQL4:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltedv/zerolte:7.0/NRD90M/G925IDVU3FQL4:user/test-keys");
        property_override("ro.build.description", "zeroltedv-user 7.0 NRD90M G925IDVU3FQL4 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G925I");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zerolte");
        property_override("ro.product.name", "zeroltedv");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-india.so");
        uses_edge_power_profile(true);
        is_cdma_device(false);

    } else if (bootloader.find("G9250") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltezc/zeroltechn:7.0/NRD90M/G9250ZCS2ERC2:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltezc/zeroltechn:7.0/NRD90M/G9250ZCS2ERC2:user/test-keys");
        property_override("ro.build.description", "zeroltezc-user 7.0 NRD90M G9250ZCS2ERC2 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G9250");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltechn");
        property_override("ro.product.name", "zeroltezc");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(true);
        is_dual_sim_device();
        is_cdma_device(true);

    } else if (bootloader.find("G9258") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltezc/zeroltechn:7.0/NRD90M/G9250ZCS2ERC2:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltezc/zeroltechn:7.0/NRD90M/G9250ZCS2ERC2:user/test-keys");
        property_override("ro.build.description", "zeroltezc-user 7.0 NRD90M G9250ZCS2ERC2 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G9250");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltechn");
        property_override("ro.product.name", "zeroltezc");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(true);
        is_dual_sim_device();
        is_cdma_device(true);

    } else if (bootloader.find("G9259") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltezc/zeroltechn:7.0/NRD90M/G9250ZCS2ERC2:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltezc/zeroltechn:7.0/NRD90M/G9250ZCS2ERC2:user/test-keys");
        property_override("ro.build.description", "zeroltezc-user 7.0 NRD90M G9250ZCS2ERC2 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G9250");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltechn");
        property_override("ro.product.name", "zeroltezc");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-spr.so");
        uses_edge_power_profile(true);
        is_dual_sim_device();
        is_cdma_device(true);

    } else if (bootloader.find("G925T") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltetmo/zeroltetmo:7.0/NRD90M/G925TUVU5FQK1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltetmo/zeroltetmo:7.0/NRD90M/G925TUVU5FQK1:user/test-keys");
        property_override("ro.build.description", "zeroltetmo-user 7.0 NRD90M G925TUVU5FQK1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G925T");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltetmo");
        property_override("ro.product.name", "zeroltetmo");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-tmobile.so");
        uses_edge_power_profile(true);
        is_cdma_device(false);

    } else if (bootloader.find("G925W8") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltebmc/zeroltebmc:7.0/NRD90M/G925W8VLS5DQK1:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltebmc/zeroltebmc:7.0/NRD90M/G925W8VLS5DQK1:user/test-keys");
        property_override("ro.build.description", "zeroltebmc-user 7.0 NRD90M G925W8VLS5DQK1 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G925W8");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltebmc");
        property_override("ro.product.name", "zeroltebmc");
        property_override("audio_hal.uses.audience", "true");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril-canada.so");
        uses_edge_power_profile(true);
        is_cdma_device(false);

    } else if (bootloader.find("G920") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zerofltexx/zeroflte:7.0/NRD90M/G920FXXU5EQL5:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zerofltexx/zeroflte:7.0/NRD90M/G920FXXU5EQL5:user/test-keys");
        property_override("ro.build.description", "zerofltexx-user 7.0 NRD90M G920FXXS5EQG2 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G920F");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroflte");
        property_override("ro.product.name", "zerofltexx");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(true);
        is_cdma_device(false);

    } else if (bootloader.find("G925") != std::string::npos) {

        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/zeroltexx/zerolte:7.0/NRD90M/G925FXXU5EQL5:user/release-keys");
        property_override("ro.bootimage.build.fingerprint", "samsung/zeroltexx/zerolte:7.0/NRD90M/G925FXXU5EQL5:user/test-keys");
        property_override("ro.build.description", "zeroltexx-user 7.0 NRD90M G925FXXU5EQL5 release-keys");
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-G925F");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "zeroltexx");
        property_override("ro.product.name", "zerolte");
        property_override("audio_hal.uses.audience", "false");
        property_override("rild.libpath", "/vendor/lib64/libsec-ril.so");
        uses_edge_power_profile(true);
        is_cdma_device(false);

    }

	device = GetProperty("ro.product.device", "");
    LOG(ERROR) << "Found bootloader id " << bootloader <<  " setting build properties for " << device << " device" << std::endl;
}
