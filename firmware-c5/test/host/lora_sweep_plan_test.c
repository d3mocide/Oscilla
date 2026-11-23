/* Host tests for lora_sweep_plan.c (WP-8). */
#include "lora_sweep_plan.h"
#include "ocp.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Default US 902-927.6 MHz and EU 863-869.3 MHz plans pass. */
    assert(lora_sweep_plan_valid(LORA_SWEEP_DEF_START_KHZ, LORA_SWEEP_DEF_STEP_KHZ, LORA_SWEEP_DEF_BINS));
    assert(lora_sweep_plan_valid(863000u, 100u, 64u));
    assert(lora_sweep_plan_valid(869000u, 400u, 64u));   /* US 850 cellular downlink */

    /* Out-of-range frequency, step, bins, and upper-bound overflow rejected. */
    assert(!lora_sweep_plan_valid(149999u, 100u, 16u));
    assert(!lora_sweep_plan_valid(960001u, 100u, 16u));
    assert(!lora_sweep_plan_valid(955000u, 400u, 64u));      /* end > 960 MHz */
    assert(!lora_sweep_plan_valid(0xFFFFFFFFu, 400u, 64u));  /* uint32 overflow guard */
    assert(!lora_sweep_plan_valid(902000u, 49u, 64u));
    assert(!lora_sweep_plan_valid(902000u, 2001u, 64u));
    assert(!lora_sweep_plan_valid(902000u, 400u, 7u));
    assert(!lora_sweep_plan_valid(902000u, 400u, OCP_LORA_SWEEP_MAX_BINS + 1u));

    /* SX1262 GetRssiInst conversion: raw / -2 dBm. */
    assert(lora_rssi_inst_to_dbm(0u) == 0);
    assert(lora_rssi_inst_to_dbm(120u) == -60);
    assert(lora_rssi_inst_to_dbm(221u) == -110);
    assert(lora_rssi_inst_to_dbm(255u) == -127);

    /* CalibrateImage bytes: 4 MHz units with a 4 MHz guard each side. */
    uint8_t img[2];
    lora_image_cal_bytes(902000u, 928000u, img);          /* datasheet upper 0xE9 */
    assert(img[0] == 0xE0 && img[1] == 0xE9);
    lora_image_cal_bytes(869525u, 869525u, img);          /* single EU RX frequency */
    assert(img[0] == 0xD8 && img[1] == 0xDB);
    lora_image_cal_bytes(894200u, 869000u, img);          /* reversed span: US cell DL */
    assert(img[0] == 0xD8 && img[1] == 0xE1);
    assert(img[0] * 4000u <= 869000u - 4000u && img[1] * 4000u >= 894200u + 4000u);
    lora_image_cal_bytes(150000u, 960000u, img);          /* chip limits stay in a byte */
    assert(img[0] == 36u && img[1] == 241u);

    /* CSV formatting and truncation guard. */
    int8_t bins[4] = {-112, -85, -120, -40};
    char buf[32];
    assert(lora_sweep_format_rssi(bins, 4, buf, sizeof buf));
    assert(strcmp(buf, "-112,-85,-120,-40") == 0);
    char small[10];
    assert(!lora_sweep_format_rssi(bins, 4, small, sizeof small));
    assert(small[0] == '\0');

    /* Worst-case 64 bins must fit comfortably inside OCP_MAX_LINE_LEN (512). */
    int8_t full[OCP_LORA_SWEEP_MAX_BINS];
    for (size_t i = 0; i < OCP_LORA_SWEEP_MAX_BINS; i++) full[i] = -127;
    char line[360];
    assert(lora_sweep_format_rssi(full, OCP_LORA_SWEEP_MAX_BINS, line, sizeof line));
    assert(strlen(line) + 80u < OCP_MAX_LINE_LEN);

    printf("lora sweep plan: span bounds, GetRssiInst conversion, image-cal bytes and formatting passed\n");
    return 0;
}
