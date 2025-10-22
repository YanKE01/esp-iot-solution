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

#define AUDIO_SAMPLE_RATE CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE
// Audio controls
// Current states
bool mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];      // +1 for master channel 0
uint16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];// +1 for master channel 0
uint32_t sampFreq;
uint8_t clkValid;

// Range states
audio_control_range_2_n_t(1) volumeRng[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];// Volume range state
audio_control_range_4_n_t(1) sampleFreqRng;                                    // Sample frequency range state

// Audio test data, 4 channels muxed together, buffer[0] for CH0, buffer[1] for CH1, buffer[2] for CH2, buffer[3] for CH3
uint16_t i2s_dummy_buffer[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX * CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE / 1000];

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

void audio_task(void)
{
    static uint32_t start_ms = 0;
    uint32_t curr_ms = esp_timer_get_time() / 1000;
    if (start_ms == curr_ms) {
        return;    // not enough time
    }
    start_ms = curr_ms;
    tud_audio_write(i2s_dummy_buffer, AUDIO_SAMPLE_RATE / 1000 * CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX);
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff)
{
    (void) rhport;
    (void) pBuff;

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t ep = TU_U16_LOW(p_request->wIndex);

    (void) channelNum;
    (void) ctrlSel;
    (void) ep;

    return false;// Yet not implemented
}

bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff)
{
    (void) rhport;
    (void) pBuff;

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);

    (void) channelNum;
    (void) ctrlSel;
    (void) itf;

    return false;// Yet not implemented
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff)
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
    if (entityID == 2) {
        switch (ctrlSel) {
        case AUDIO_FU_CTRL_MUTE:
            // Request uses format layout 1
            TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));

            mute[channelNum] = ((audio_control_cur_1_t *) pBuff)->bCur;

            ESP_LOGI(TAG, "    Set Mute: %d of channel: %u", mute[channelNum], channelNum);
            return true;

        case AUDIO_FU_CTRL_VOLUME:
            // Request uses format layout 2
            TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));

            volume[channelNum] = (uint16_t)((audio_control_cur_2_t *) pBuff)->bCur;

            ESP_LOGI(TAG, "    Set Volume: %d dB of channel: %u", volume[channelNum], channelNum);
            return true;

        // Unknown/Unsupported control
        default:
            TU_BREAKPOINT();
            return false;
        }
    }
    return false;// Yet not implemented
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void) rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t ep = TU_U16_LOW(p_request->wIndex);

    (void) channelNum;
    (void) ctrlSel;
    (void) ep;

    //  return tud_control_xfer(rhport, p_request, &tmp, 1);

    return false;// Yet not implemented
}

bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void) rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);

    (void) channelNum;
    (void) ctrlSel;
    (void) itf;

    return false;// Yet not implemented
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void) rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    // uint8_t itf = TU_U16_LOW(p_request->wIndex);             // Since we have only one audio function implemented, we do not need the itf value
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

    // Input terminal (Microphone input)
    if (entityID == 1) {
        switch (ctrlSel) {
        case AUDIO_TE_CTRL_CONNECTOR: {
            // The terminal connector control only has a get request with only the CUR attribute.
            audio_desc_channel_cluster_t ret;

            // Those are dummy values for now
            ret.bNrChannels = 1;
            ret.bmChannelConfig = (audio_channel_config_t) 0;
            ret.iChannelNames = 0;

            ESP_LOGI(TAG, "    Get terminal connector");

            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void *) &ret, sizeof(ret));
        } break;

        // Unknown/Unsupported control selector
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    // Feature unit
    if (entityID == 2) {
        switch (ctrlSel) {
        case AUDIO_FU_CTRL_MUTE:
            // Audio control mute cur parameter block consists of only one byte - we thus can send it right away
            // There does not exist a range parameter block for mute
            ESP_LOGI(TAG, "    Get Mute of channel: %u", channelNum);
            return tud_control_xfer(rhport, p_request, &mute[channelNum], 1);

        case AUDIO_FU_CTRL_VOLUME:
            switch (p_request->bRequest) {
            case AUDIO_CS_REQ_CUR:
                ESP_LOGI(TAG, "    Get Volume of channel: %u", channelNum);
                return tud_control_xfer(rhport, p_request, &volume[channelNum], sizeof(volume[channelNum]));

            case AUDIO_CS_REQ_RANGE:
                ESP_LOGI(TAG, "    Get Volume range of channel: %u", channelNum);

                // Copy values - only for testing - better is version below
                audio_control_range_2_n_t(1)
                ret;

                ret.wNumSubRanges = 1;
                ret.subrange[0].bMin = -90;// -90 dB
                ret.subrange[0].bMax = 90; // +90 dB
                ret.subrange[0].bRes = 1;  // 1 dB steps

                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void *) &ret, sizeof(ret));

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
    if (entityID == 4) {
        switch (ctrlSel) {
        case AUDIO_CS_CTRL_SAM_FREQ:
            // channelNum is always zero in this case
            switch (p_request->bRequest) {
            case AUDIO_CS_REQ_CUR:
                ESP_LOGI(TAG, "    Get Sample Freq.");
                // Buffered control transfer is needed for IN flow control to work
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));

            case AUDIO_CS_REQ_RANGE:
                ESP_LOGI(TAG, "    Get Sample Freq. range");
                return tud_control_xfer(rhport, p_request, &sampleFreqRng, sizeof(sampleFreqRng));

            // Unknown/Unsupported control
            default:
                TU_BREAKPOINT();
                return false;
            }
            break;

        case AUDIO_CS_CTRL_CLK_VALID:
            // Only cur attribute exists for this request
            ESP_LOGI(TAG, "    Get Sample Freq. valid");
            return tud_control_xfer(rhport, p_request, &clkValid, sizeof(clkValid));

        // Unknown/Unsupported control
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    ESP_LOGI(TAG, "  Unsupported entity: %d", entityID);
    return false;// Yet not implemented
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

    // Init values
    sampFreq = AUDIO_SAMPLE_RATE;
    clkValid = 1;

    sampleFreqRng.wNumSubRanges = 1;
    sampleFreqRng.subrange[0].bMin = AUDIO_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bMax = AUDIO_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bRes = 0;

    // Generate dummy data
    uint16_t *p_buff = i2s_dummy_buffer;
    uint16_t dataVal = 0;
    for (uint16_t cnt = 0; cnt < AUDIO_SAMPLE_RATE / 1000; cnt++) {
        // CH0 saw wave
        *p_buff++ = dataVal;
        // CH1 inverted saw wave
        *p_buff++ = 3200 + AUDIO_SAMPLE_RATE / 1000 - dataVal;
        dataVal += 32;
        // CH3 square wave
        *p_buff++ = cnt < (AUDIO_SAMPLE_RATE / 1000 / 2) ? 3400 : 5000;
        // CH4 sinus wave
        float t = 2 * 3.1415f * cnt / (AUDIO_SAMPLE_RATE / 1000);
        *p_buff++ = (uint16_t)((int16_t)(sin(t) * 750) + 6000);
    }

    BaseType_t ret_val = xTaskCreatePinnedToCore(tusb_device_task, "TinyUSB", 7 * 1024, NULL, 5, NULL, tskNO_AFFINITY);
    if (ret_val != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TinyUSB task");
        return;
    }

}
