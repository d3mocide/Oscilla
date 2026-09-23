/*
 * deck_app.h — screen flow and command orchestration for the deck.
 *
 *   Grouped route order follows docs/brand/README.md:
 *   SYSTEM -> OBSERVE -> ANALYZE -> DRIVE. AP Detail is a Wi-Fi Scan drill-down.
 *   ` = stop + back, except on Link where it opens Settings (DESIGN §7.3)
 *
 * AP Detail is the only drill-down; defensive analysis cards are first-class
 * home routes in DESIGN §7.2.
 *
 * PHY tool changes wait for `stop phy` (D-16, OCP-SPEC §5.4), preserving
 * a concurrent LoRa receiver.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "app/deck_navigation.h"
#include "app/phy_handoff.h"
#include "gnss/nmea_parser.h"
#include "model/bt_model.h"
#include "model/contacts_model.h"
#include "model/deauth_model.h"
#include "model/anti_surveillance_model.h"
#include "model/gnss_model.h"
#include "model/lora_model.h"
#include "model/scan_model.h"
#include "model/spectrum_model.h"
#include "model/zig_model.h"
#include "ocp/ocp_client.h"
#include "ui/contacts_view.h"
#include "ui/zig_view.h"

namespace app {

struct Keys {
    std::string chars;   /* printable keys pressed this frame */
    bool enter = false;
};

class DeckApp {
public:
    explicit DeckApp(ocp::Client::Write write);

    void begin(uint32_t now_ms);
    void feed(const uint8_t *data, size_t len, uint32_t now_ms) { now_ = now_ms; client_.feed(data, len, now_ms); }

    /* GNSS bytes from the deck's own UART. Never crosses OCP (DESIGN §9.1):
     * a separate wire, a separate parser, no framing in common. */
    void feedGnss(const uint8_t *data, size_t len, uint32_t now_ms);
    void tick(uint32_t now_ms);
    void onKeys(const Keys &keys, uint32_t now_ms);

    /* Debug console (DESIGN §7.6): named commands over the same USB Serial
     * the diagnostic log already uses, independent of the current screen —
     * a bench script shouldn't have to track cursor/screen state the way a
     * human at the keyboard does. Off by default. Unrecognized/malformed
     * input is echoed back over log(), never silently swallowed — a script
     * needs to know a command landed. */
    bool debugEnabled() const { return debug_mode_; }
    /* RAM only, no SD write — main.cpp uses this once at boot to apply
     * whatever storage::loadDebugMode() already read back. */
    void setDebugMode(bool on) { debug_mode_ = on; }
    /* 'd', bound globally in onKeys(): flips the flag, persists it via
     * storage::saveDebugMode() so it survives a reflash (the SD card
     * remembers it, not the firmware image), and leaves a notice either way. */
    void toggleDebugMode();
    void runDebugCommand(const std::string &line, uint32_t now_ms);

    /* Display brightness (DESIGN §7.6), M5GFX 0-255 scale. main.cpp reads
     * storage::loadBrightness() and applies it to the hardware before
     * begin() runs (display setup is main.cpp's wiring job, same split as
     * setDebugMode()); this just seeds the in-RAM value the Settings
     * screen renders and adjusts from there on, persisting via
     * storage::saveBrightness() the same way toggleDebugMode() does. */
    uint8_t brightness() const { return brightness_; }
    void setBrightness(uint8_t value) { brightness_ = value; }

    /* True when the screen should be redrawn. */
    bool dirty(uint32_t now_ms) const;
    void draw(uint32_t now_ms);

    const ocp::Client &client() const { return client_; }

    /* Diagnostics sink. Counts and states only: never SSIDs or BSSIDs. */
    void onLog(std::function<void(const std::string &)> log) { log_ = std::move(log); }

private:
    void onReply(const ocp::Item &it);
    void onEvent(const ocp::Item &it);
    /* The wire-level half of startScan(): send scan_networks, reset scan_.
     * No screen/cursor change - this is what the wardrive auto-loop calls
     * so it can re-trigger a scan every ~10s without yanking the view back
     * to Wi-Fi Scan each time. */
    void requestScan(uint32_t now_ms);
    void startScan(uint32_t now_ms);
    /* Engines actually receiving right now, no pending-start flags — the
     * chrome RF/LIVE indicator wants this so it doesn't light up before a
     * radio is truly live. phyToolActive() (below) is the superset used to
     * decide whether a new PHY request must stop the current one first. */
    bool phyEngineActive() const;
    bool phyToolActive() const;
    bool preparePhyStart(PhyHandoff::Start start, uint32_t now_ms);
    void stopPhy(uint32_t now_ms);
    void stopPhyForNavigation(uint32_t now_ms);
    void toggleWifiContinuous(uint32_t now_ms);
    void startInspect(uint32_t now_ms);
    void startSniffer(uint32_t now_ms);
    void startChannelView(uint32_t now_ms);
    void startPacketMonitor(uint32_t now_ms, uint8_t ch);
    void startLoraConfig(uint32_t now_ms);
    void sendLoraConfig(uint32_t freq_hz, int sf, int bw_khz, int cr, uint8_t sync_word,
                       const char *profile_token, uint32_t now_ms);
    void startLoraListen(uint32_t now_ms);
    void startDeauthDetector(uint32_t now_ms);
    /* The three BLE engines (scan_bt / start_ble_scan / scan_airtag) share
     * one radio mode, so starting one requires the other two to be idle. */
    enum class BleMode : uint8_t { Scan, Continuous, Airtag };
    bool bleBusyElsewhere(BleMode mine) const;
    void startBtScan(uint32_t now_ms);
    void toggleBtContinuous(uint32_t now_ms);
    void toggleAirtagScan(uint32_t now_ms);
    void toggleAntisurveillance(uint32_t now_ms);
    void toggleZig(uint32_t now_ms);
    void toggleWardriveLog(uint32_t now_ms);
    void logScanRows();
    void back(uint32_t now_ms);
    void notice(const std::string &text);
    /* ';'/'.' on the Settings screen: clamps to [kBrightnessMin,
     * kBrightnessMax] (never 0 — a black screen has no way back), applies
     * live via M5Cardputer.Display.setBrightness(), and persists. */
    void adjustBrightness(int direction);

    /* A command couldn't even be sent yet (client_.pending() from something
     * else in flight, not a real conflict) - retry it once that clears,
     * same idea as the scan-paging retry in tick(), generalized. Bounded so
     * a stuck link degrades to an error, not a silent forever-retry. */
    void retrySoon(std::function<void(uint32_t)> action, uint32_t now_ms);

    ocp::Client client_;
    model::ScanModel scan_;
    model::ContactsModel contacts_;
    model::SpectrumModel spectrum_;
    model::LoraModel lora_;
    model::DeauthModel deauth_;
    model::BtModel bt_;
    model::AntiSurveillanceModel anti_;
    model::ZigModel zig_;
    gnss::NmeaParser gnss_parser_;
    model::GnssModel gnss_;
    /* Track vertices are sampled, not written per sentence: a 1 Hz fix for
     * an hour is 3600 points, and the track only needs enough to draw. */
    uint32_t last_track_point_ms_ = 0;
    uint32_t last_gnss_diag_ms_ = 0;
    bool debug_mode_ = false;
    /* How much of scan_.rows() the wardrive log has already consumed —
     * [SCAN] arrives paged, and rows() accumulates across pages. */
    size_t wardrive_logged_upto_ = 0;
    Screen screen_ = Screen::Link;
    size_t link_cursor_ = 0;
    size_t cursor_ = 0;
    uint16_t trace_idx_ = 0;
    size_t contacts_cursor_ = 0;
    ui::ContactsTab contacts_tab_ = ui::ContactsTab::Clients;
    bool contacts_poll_clients_ = true;
    uint32_t last_contacts_poll_ms_ = 0;
    size_t spectrum_cursor_ = 0;
    uint32_t spectrum_started_ms_ = 0;
    size_t lora_cursor_ = 0;
    uint32_t last_lora_health_poll_ms_ = 0;
    /* Which model::kLoraProfiles[] entry 'c' would apply next — cycled with
     * 'x', independent of whichever profile is actually configured/running
     * (lora_.profile()) until 'c' is pressed again. */
    size_t lora_profile_index_ = 0;
    /* A rejected lora_listen (e.g. sent before lora_config) must not create
     * a session file — the ack only exists after the probe actually
     * confirms, so the [LORA]/error reply is what triggers lora_.begin()
     * and storage::loraLogBegin(), not the optimistic send. */
    bool lora_listen_pending_ = false;
    /* Same rule for lora_config: the model (and so the display and the next
     * session manifest) only takes the new parameters once [CFG] confirms
     * the probe accepted them. A rejected config leaves both sides on their
     * previous values. */
    struct PendingLoraConfig {
        uint32_t freq_hz;
        int sf;
        int bw_khz;
        int cr;
        uint8_t sync_word;
        const char *profile_token;   /* static storage: kLoraProfiles or a literal */
    };
    bool lora_config_pending_ = false;
    PendingLoraConfig lora_config_sent_{};
    size_t deauth_cursor_ = 0;
    size_t anti_cursor_ = 0;
    size_t bt_cursor_ = 0;
    uint32_t bt_scan_started_ms_ = 0;
    size_t zig_cursor_ = 0;
    ui::ZigTab zig_tab_ = ui::ZigTab::Nodes;
    uint32_t last_zig_poll_ms_ = 0;
    bool zig_start_pending_ = false;
    bool anti_start_pending_ = false;
    /* BLE starts are acknowledged asynchronously. Keep these separate from
     * the model's running flags so a rejected PHY start cannot look live. */
    bool bt_scan_pending_ = false;
    bool bt_continuous_pending_ = false;
    bool bt_airtag_pending_ = false;
    bool spectrum_start_pending_ = false;
    PhyHandoff phy_handoff_;

    /* Queued because the client couldn't send yet (something else was
     * still pending) - retried once that clears, see retrySoon(). */
    std::function<void(uint32_t)> pending_retry_;
    uint32_t pending_retry_started_ms_ = 0;

    bool probe_status_valid_ = false;
    uint32_t probe_heap_ = 0;
    uint32_t probe_heap_total_ = 0;
    uint32_t probe_heap_min_ = 0;
    uint32_t probe_heap_largest_ = 0;
    uint32_t probe_psram_total_ = 0;
    uint32_t probe_psram_free_ = 0;
    uint32_t probe_psram_largest_ = 0;
    uint64_t probe_uptime_ms_ = 0;
    uint32_t last_status_reply_ms_ = 0;
    uint32_t last_status_poll_ms_ = 0;

    std::string last_reply_ = "-";
    std::string notice_;
    uint32_t notice_started_ms_ = 0;
    bool help_visible_ = false;
    /* Placeholder only — main.cpp overwrites this via setBrightness() at
     * boot from storage::loadBrightness(), same as debug_mode_ above. */
    uint8_t brightness_ = 160;
    uint16_t next_page_ = 0;
    uint32_t scan_started_ms_ = 0;
    bool scan_pending_ = false;
    bool wifi_continuous_pending_ = false;
    uint32_t last_draw_ms_ = 0;
    /* M5.Power is an I2C read; cache it instead of hitting the bus on every
     * chrome redraw (draw() can run every kBusyRedrawMs while a tool is
     * active). */
    int cached_battery_pct_ = -1;
    bool cached_charging_ = false;
    uint32_t last_battery_poll_ms_ = 0;
    bool battery_polled_ = false;
    uint32_t last_attempt_ms_ = 0;
    uint32_t last_keepalive_ms_ = 0;
    uint32_t now_ = 0;
    bool dirty_ = true;
    std::function<void(const std::string &)> log_;
    void log(const std::string &line) { if (log_) log_(line); }
};

}  // namespace app
