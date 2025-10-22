/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sdkconfig.h"

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device can be defined by board.mk, default to port 0
#ifdef CONFIG_TINYUSB_RHPORT_HS
#   define CFG_TUSB_RHPORT1_MODE    OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED
#   define CONFIG_USB_HS            1
#else
#   define CFG_TUSB_RHPORT0_MODE    OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED
#   define CONFIG_USB_HS            0
#endif

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_FREERTOS
#endif

#ifndef ESP_PLATFORM
#define ESP_PLATFORM 1
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

#if TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3, OPT_MCU_ESP32P4)
#define CFG_TUSB_OS_INC_PATH    freertos/
#endif

// Enable Device stack
#define CFG_TUD_ENABLED       1

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

//------------- CLASS -------------//
#define CFG_TUD_AUDIO             1
#define CFG_TUD_CDC               0
#define CFG_TUD_MSC               0
#define CFG_TUD_HID               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0

//--------------------------------------------------------------------
// AUDIO CLASS DRIVER CONFIGURATION
//--------------------------------------------------------------------

// Have a look into audio_device.h for all configurations

#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE              48000

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                 TUD_AUDIO_MIC_FOUR_CH_DESC_LEN

#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT                 1
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ              64

#define CFG_TUD_AUDIO_ENABLE_EP_IN                    1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX    2         // This value is not required by the driver, it parses this information from the descriptor once the alternate interface is set by the host - we use it for the setup
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX            4         // This value is not required by the driver, it parses this information from the descriptor once the alternate interface is set by the host - we use it for the setup
#define CFG_TUD_AUDIO_EP_SZ_IN                        TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)

#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL              1

#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX             CFG_TUD_AUDIO_EP_SZ_IN
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ          (TUD_OPT_HIGH_SPEED ? 32 : 4) * CFG_TUD_AUDIO_EP_SZ_IN // Example write FIFO every 1ms, so it should be 8 times larger for HS device

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
