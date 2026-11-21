/* Host tests for model::LoraProfileCatalog (WP-9a). */
#include "model/lora_profile_catalog.h"

#include <cassert>
#include <iostream>

int main()
{
    model::LoraProfileCatalog cat;
    assert(cat.size() == model::kLoraProfileCount);
    assert(cat.sdCount() == 0 && cat.rejectedRows() == 0);
    assert(cat.at(0).token == "meshcore_us_ca" && !cat.at(0).from_sd);
    assert(cat.findByToken("meshtastic_us_longfast") == 1);
    assert(cat.findByToken("unknown_token") == 0);
    assert(cat.labelForToken("meshcore_us_ca") == "MCORE");
    assert(cat.labelForToken("manual") == "MANUAL");

    // Valid CSV with header, comments, CRLF, hex and decimal sync words.
    const std::string csv =
        "# Custom LoRa presets\r\n"
        "token,label,freq_hz,sf,bw_khz,cr,sync_word\r\n"
        "eu868_mesh,EU868 MESH,869525000,11,250,1,0x2B\r\n"
        "us915_lorawan,US915 LW0,902300000,7,125,1,52\n"
        "\n"
        "meshcore_us_ca,DUP BUILTIN,910525000,7,62,1,0x12\n"       // duplicate built-in -> rejected
        "eu868_mesh,DUP SD,869525000,11,250,1,0x2B\n"              // duplicate SD -> rejected
        "bad_freq_lo,TOO LOW,149999999,7,125,1,0x12\n"             // <150 MHz -> rejected
        "bad_freq_hi,TOO HIGH,960000001,7,125,1,0x12\n"            // >960 MHz -> rejected
        "bad_sf,SF13,915000000,13,125,1,0x12\n"                    // SF>12 -> rejected
        "bad_bw,BW100,915000000,7,100,1,0x12\n"                    // invalid SX1262 BW -> rejected
        "bad_cr,CR5,915000000,7,125,5,0x12\n"                      // CR>4 -> rejected
        "bad_sync,SYNC256,915000000,7,125,1,0x100\n"               // >0xFF -> rejected
        "bad tok,SPACE,915000000,7,125,1,0x12\n"                   // space in token -> rejected
        "ctrl_lbl,BAD\x07LBL,915000000,7,125,1,0x12\n"             // control char -> rejected
        "extra_col,EXTRA,915000000,7,125,1,0x12,99\n"              // >7 cols -> rejected
        + std::string(300, 'X') + "\n";                            // overlong -> rejected

    const size_t added = cat.parseCsv(csv);
    assert(added == 2);
    assert(cat.sdCount() == 2);
    assert(cat.size() == model::kLoraProfileCount + 2);
    assert(cat.rejectedRows() == 12);
    assert(cat.findByToken("eu868_mesh") == 2);
    assert(cat.at(2).from_sd && cat.at(2).freq_hz == 869525000 && cat.at(2).sync_word == 0x2B);
    assert(cat.findByToken("us915_lorawan") == 3);
    assert(cat.at(3).from_sd && cat.at(3).sync_word == 52);
    assert(cat.labelForToken("eu868_mesh") == "EU868 MESH");

    // Cap enforcement at kMaxProfiles (16 total).
    std::string many;
    for (int i = 0; i < 30; ++i) {
        many += "p_" + std::to_string(i) + ",P" + std::to_string(i) + ",915000000,7,125,1,0x12\n";
    }
    cat.parseCsv(many);
    assert(cat.size() == model::LoraProfileCatalog::kMaxProfiles);

    std::cout << "lora profile catalog: builtins, SD CSV validation, hostile rejections and 16-profile cap passed\n";
    return 0;
}
