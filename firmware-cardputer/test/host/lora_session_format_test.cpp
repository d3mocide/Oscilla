/* lora_session_format_test.cpp — host test for LoRa sidecar formats. */

#include <cstdio>
#include <string>

#include "model/lora_model.h"
#include "storage/lora_session_format.h"

int main()
{
    int failures = 0;
    model::LoraModel lora;
    lora.configured(910525000, 7, 62, 1, 0x12, "meshcore_us_ca");
    const std::string manifest = storage::loraSessionManifest(1234, lora);
    const bool manifest_ok = manifest == "schema=oscilla-lora-session-v1\nstarted_ms=1234\nprofile=meshcore_us_ca\nfreq_hz=910525000\nsf=7\nbw_khz=62\ncr=1\nsync_word=0x12\n";
    std::printf("  %s  manifest records profile and exact configuration\n", manifest_ok ? "PASS" : "FAIL");
    failures += !manifest_ok;

    model::LoraHealth h;
    h.valid = true; h.rx = 3; h.crc_err = 1; h.header_err = 2; h.irq_drop = 4; h.radio_drop = 5;
    h.ocp_drop = 6; h.hw_fault = 7;
    const bool header_ok = storage::loraHealthHeader() ==
        "ts_ms,rx,crc_err,header_err,irq_drop,radio_drop,ocp_drop,hw_fault,packet_rows,packet_drops,health_drops\n";
    const bool row_ok = storage::loraHealthRow(99, h, 8, 9, 10) == "99,3,1,2,4,5,6,7,8,9,10\n";
    std::printf("  %s  health sidecar schema is stable\n", header_ok && row_ok ? "PASS" : "FAIL");
    failures += !(header_ok && row_ok);
    std::printf("\n%s\n", failures ? "lora session format test FAILED" : "lora session format test OK");
    return failures ? 1 : 0;
}
