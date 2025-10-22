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
#include "usb_descriptors.h"

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

#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE                         96000

// How many formats are used, need to adjust USB descriptor if changed
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS                               2

// 16bit in 16bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX          2
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX                  16

// 24bit in 32bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_2_N_BYTES_PER_SAMPLE_TX          4
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_2_RESOLUTION_RX                  24

// Have a look into audio_device.h for all configurations

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                                 TUD_AUDIO_MIC_ONE_CH_2_FORMAT_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT                                 1                                       // Number of Standard AS Interface Descriptors (4.9.1) defined per audio function - this is required to be able to remember the current alternate settings of these interfaces - We restrict us here to have a constant number for all audio functions (which means this has to be the maximum number of AS interfaces an audio function has and a second audio function with less AS interfaces just wastes a few bytes)
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ                              64                                      // Size of control request buffer

#define CFG_TUD_AUDIO_ENABLE_EP_IN                                    1
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                            1                                       // Driver gets this info from the descriptors - we define it here to use it to setup the descriptors and to do calculations with it below - be aware: for different number of channels you need another descriptor!

#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN                        TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_2_EP_SZ_IN                        TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_FORMAT_2_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)

#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX                             TU_MAX(CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN, CFG_TUD_AUDIO_FUNC_1_FORMAT_2_EP_SZ_IN)                  // Maximum EP IN size for all AS alternate settings used
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ                          CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX
#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
