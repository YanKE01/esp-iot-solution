/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_private/usb_phy.h"
#include "tusb.h"
#include "esp_timer.h"
#include "math.h"

static const char *TAG = "uac_test";
static usb_phy_handle_t phy_hdl;

static void usb_phy_init(void)
{
    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .target = USB_PHY_TARGET_INT,
#if CONFIG_TINYUSB_RHPORT_HS
        .otg_speed = USB_PHY_SPEED_HIGH,
#endif
    };
    usb_new_phy(&phy_conf, &phy_hdl);
}

// Invoked when device is mounted
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB Mounted");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB Unmounted");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    ESP_LOGI(TAG, "USB Suspended");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB Resumed");
}

// Audio controls
// Current states
bool mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];                        // +1 for master channel 0
uint16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];                    // +1 for master channel 0
uint32_t sampFreq;
uint8_t bytesPerSample;
uint8_t clkValid;

// Range states
// List of supported sample rates
static const uint32_t sampleRatesList[] = {
    32000, 48000, 96000
};

#define N_sampleRates  TU_ARRAY_SIZE(sampleRatesList)

// Bytes per format of every Alt settings
static const uint8_t bytesPerSampleAltList[CFG_TUD_AUDIO_FUNC_1_N_FORMATS] = {
    CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX,
    CFG_TUD_AUDIO_FUNC_1_FORMAT_2_N_BYTES_PER_SAMPLE_TX,
};

audio_control_range_2_n_t(1) volumeRng[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];         // Volume range state

// Audio test data
CFG_TUD_MEM_ALIGN uint8_t test_buffer_audio[CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX];
uint16_t startVal = 0;

void audio_task(void)
{
    // Yet to be filled - e.g. put meas data into TX FIFOs etc.
    // asm("nop");
}

// Invoked when set interface is called, typically on start/stop streaming or format change
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    (void)rhport;
    //uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex));
    uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));

    ESP_LOGI(TAG, "Set interface alt %d", alt);

    // Clear buffer when streaming format is changed
    if (alt != 0) {
        bytesPerSample = bytesPerSampleAltList[alt - 1];
    }
    return true;
}

// Invoked when audio class specific set request received for an EP
bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff)
{
    (void) rhport;
    (void) pBuff;

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t ep = TU_U16_LOW(p_request->wIndex);

    (void) channelNum; (void) ctrlSel; (void) ep;

    return false;     // Yet not implemented
}

// Invoked when audio class specific set request received for an interface
bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff)
{
    (void) rhport;
    (void) pBuff;

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);

    (void) channelNum; (void) ctrlSel; (void) itf;

    return false;     // Yet not implemented
}

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff)
{
    (void) rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

    (void) itf;

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // If request is for our feature unit
    if (entityID == UAC2_ENTITY_FEATURE_UNIT) {
        switch (ctrlSel) {
        case AUDIO_FU_CTRL_MUTE:
            // Request uses format layout 1
            TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));

            mute[channelNum] = ((audio_control_cur_1_t*) pBuff)->bCur;

            TU_LOG2("    Set Mute: %d of channel: %u\r\n", mute[channelNum], channelNum);
            return true;

        case AUDIO_FU_CTRL_VOLUME:
            // Request uses format layout 2
            TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));

            volume[channelNum] = (uint16_t)((audio_control_cur_2_t*) pBuff)->bCur;

            TU_LOG2("    Set Volume: %d dB of channel: %u\r\n", volume[channelNum], channelNum);
            return true;

        // Unknown/Unsupported control
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    // Clock Source unit
    if (entityID == UAC2_ENTITY_CLOCK) {
        switch (ctrlSel) {
        case AUDIO_CS_CTRL_SAM_FREQ:
            TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_4_t));

            sampFreq = (uint32_t)((audio_control_cur_4_t *)pBuff)->bCur;

            TU_LOG2("Clock set current freq: %" PRIu32 "\r\n", sampFreq);

            return true;
            break;

        // Unknown/Unsupported control
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    return false;    // Yet not implemented
}

// Invoked when audio class specific get request received for an EP
bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    (void) rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t ep = TU_U16_LOW(p_request->wIndex);

    (void) channelNum; (void) ctrlSel; (void) ep;

    //    return tud_control_xfer(rhport, p_request, &tmp, 1);

    return false;     // Yet not implemented
}

// Invoked when audio class specific get request received for an interface
bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    (void) rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);

    (void) channelNum; (void) ctrlSel; (void) itf;

    return false;     // Yet not implemented
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    (void) rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    // uint8_t itf = TU_U16_LOW(p_request->wIndex);           // Since we have only one audio function implemented, we do not need the itf value
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

    // Input terminal (Microphone input)
    if (entityID == UAC2_ENTITY_INPUT_TERMINAL) {
        switch (ctrlSel) {
        case AUDIO_TE_CTRL_CONNECTOR: {
            // The terminal connector control only has a get request with only the CUR attribute.
            audio_desc_channel_cluster_t ret;

            // Those are dummy values for now
            ret.bNrChannels = 1;
            ret.bmChannelConfig = 0;
            ret.iChannelNames = 0;

            TU_LOG2("    Get terminal connector\r\n");

            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*) &ret, sizeof(ret));
        }
        break;

        // Unknown/Unsupported control selector
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    // Feature unit
    if (entityID == UAC2_ENTITY_FEATURE_UNIT) {
        switch (ctrlSel) {
        case AUDIO_FU_CTRL_MUTE:
            // Audio control mute cur parameter block consists of only one byte - we thus can send it right away
            // There does not exist a range parameter block for mute
            TU_LOG2("    Get Mute of channel: %u\r\n", channelNum);
            return tud_control_xfer(rhport, p_request, &mute[channelNum], 1);

        case AUDIO_FU_CTRL_VOLUME:
            switch (p_request->bRequest) {
            case AUDIO_CS_REQ_CUR:
                TU_LOG2("    Get Volume of channel: %u\r\n", channelNum);
                return tud_control_xfer(rhport, p_request, &volume[channelNum], sizeof(volume[channelNum]));

            case AUDIO_CS_REQ_RANGE:
                TU_LOG2("    Get Volume range of channel: %u\r\n", channelNum);

                // Copy values - only for testing - better is version below
                audio_control_range_2_n_t(1)
                ret;

                ret.wNumSubRanges = 1;
                ret.subrange[0].bMin = -90;    // -90 dB
                ret.subrange[0].bMax = 30;      // +30 dB
                ret.subrange[0].bRes = 1;       // 1 dB steps

                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*) &ret, sizeof(ret));

            // Unknown/Unsupported control
            default:
                TU_BREAKPOINT();
                return false;
            }
            break;

        // Unknown/Unsupported control
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    // Clock Source unit
    if (entityID == UAC2_ENTITY_CLOCK) {
        switch (ctrlSel) {
        case AUDIO_CS_CTRL_SAM_FREQ:
            // channelNum is always zero in this case
            switch (p_request->bRequest) {
            case AUDIO_CS_REQ_CUR:
                TU_LOG2("    Get Sample Freq.\r\n");
                return tud_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));

            case AUDIO_CS_REQ_RANGE: {
                TU_LOG2("    Get Sample Freq. range\r\n");
                audio_control_range_4_n_t(N_sampleRates) rangef = {
                    .wNumSubRanges = tu_htole16(N_sampleRates)
                };
                TU_LOG1("Clock get %d freq ranges\r\n", N_sampleRates);
                for (uint8_t i = 0; i < N_sampleRates; i++) {
                    rangef.subrange[i].bMin = (int32_t)sampleRatesList[i];
                    rangef.subrange[i].bMax = (int32_t)sampleRatesList[i];
                    rangef.subrange[i].bRes = 0;
                    TU_LOG1("Range %d (%d, %d, %d)\r\n", i, (int)rangef.subrange[i].bMin, (int)rangef.subrange[i].bMax, (int)rangef.subrange[i].bRes);
                }
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &rangef, sizeof(rangef));
            }
            // Unknown/Unsupported control
            default:
                TU_BREAKPOINT();
                return false;
            }
            break;

        case AUDIO_CS_CTRL_CLK_VALID:
            // Only cur attribute exists for this request
            TU_LOG2("    Get Sample Freq. valid\r\n");
            return tud_control_xfer(rhport, p_request, &clkValid, sizeof(clkValid));

        // Unknown/Unsupported control
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    TU_LOG2("  Unsupported entity: %d\r\n", entityID);
    return false;     // Yet not implemented
}

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting)
{
    (void) rhport;
    (void) itf;
    (void) ep_in;
    (void) cur_alt_setting;

    tud_audio_write((uint8_t *)test_buffer_audio, (uint16_t)(sampFreq / (TUD_OPT_HIGH_SPEED ? 8000 : 1000) * bytesPerSample));

    return true;
}

bool tud_audio_tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting)
{
    (void) rhport;
    (void) n_bytes_copied;
    (void) itf;
    (void) ep_in;
    (void) cur_alt_setting;

    // 16bit
    if (bytesPerSample == 2) {
        uint16_t* pData_16 = (uint16_t*)((void*)test_buffer_audio);
        for (size_t cnt = 0; cnt < sampFreq / (TUD_OPT_HIGH_SPEED ? 8000 : 1000); cnt++) {
            pData_16[cnt] = startVal++;
        }
    }
    // 24bit in 32bit slot
    else if (bytesPerSample == 4) {
        uint32_t* pData_32 = (uint32_t*)((void*)test_buffer_audio);
        for (size_t cnt = 0; cnt < sampFreq / (TUD_OPT_HIGH_SPEED ? 8000 : 1000); cnt++) {
            pData_32[cnt] = (uint32_t)startVal++ << 16U;
        }
    }

    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    (void) rhport;
    (void) p_request;
    startVal = 0;

    return true;
}

static void tusb_device_task(void *arg)
{
    while (1) {
        tud_task();
        audio_task();
    }
}

void app_main(void)
{
    usb_phy_init();
    bool usb_init = tusb_init();
    if (!usb_init) {
        ESP_LOGE(TAG, "USB Device Stack Init Fail");
        return;
    }

    BaseType_t ret_val = xTaskCreatePinnedToCore(tusb_device_task, "TinyUSB", 7 * 1024, NULL, 5, NULL, tskNO_AFFINITY);
    if (ret_val != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TinyUSB task");
        return;
    }

}
