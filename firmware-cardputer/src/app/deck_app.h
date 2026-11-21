/*
 * deck_app.h — screen flow and command orchestration for the deck.
 *
 *   Grouped route order follows docs/brand/README.md:
 *   SYSTEM -> OBSERVE -> ANALYZE -> DRIVE. AP Detail is a Wi-Fi Scan drill-down.
 *   ` = stop + back, except on Link where it opens Settings (DESIGN §7.3)
 *
 * AP Detail and packet inspection are drill-downs; defensive analysis cards are first-class
 * home routes in DESIGN §7.2.
 *
 * PHY tool changes wait for `stop phy` (D-16, OCP-SPEC §5.4), preserving
 * a concurrent LoRa receiver.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "model/system_status.h"
#include "model/inspection_state.h"
#include "model/contact_inspection.h"
#include "model/device_inspection.h"
#include "model/gnss_noise.h"

#include <cstdint>
#include <functional>
#include <string>

#include "app/deck_navigation.h"
#include "model/session_review.h"
#include "app/system_navigation.h"
#include "app/link_reconcile.h"
#include "app/wifi_region_sync.h"
#include "app/wifi_band_sync.h"
#include "app/phy_handoff.h"
#include "gnss/nmea_parser.h"
#include "model/bt_model.h"
#include "model/contacts_model.h"
#include "model/deauth_model.h"
#include "model/anti_surveillance_model.h"
#include "model/gnss_model.h"
#include "model/drive_trail.h"
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
    const model::GnssModel &gnssModel() const { return gnss_; }
    const model::SpectrumModel &spectrumModel() const { return spectrum_; }
    size_t spectrumCursor() const { return spectrum_cursor_; }
    bool showingSessions()const{return screen_==Screen::Logs;}
    const model::SessionReview &sessionReview()const{return session_review_;}
    bool showingSpectrum() const { return screen_ == Screen::Spectrum; }
    bool showingWifi() const { return screen_ == Screen::Sweep; }
    const model::ScanModel &scanModel() const { return scan_; }
    size_t scanCursor() const { return cursor_; }
    bool showingTrace() const { return screen_ == Screen::Trace; }
    const model::WifiInspection &wifiInspection()const{return wifi_inspection_;}
    const model::BtInspection &btInspection()const{return bt_inspection_;}
    bool traceListening() const { return client_.pending() && client_.pendingVerb() == OCP_V_INSPECT_NETWORK; }
    const model::LoraModel &loraModel() const { return lora_; }
    const model::PacketInspection &packetInspection() const { return packet_inspection_; }
    size_t loraCursor() const { return lora_cursor_; }
    const model::DriveTrail &driveTrail() const { return drive_trail_; }
    bool wardriveClosing()const{return wardrive_closing_;}
    bool showingDrive() const { return screen_ == Screen::Drive; }
    bool showingDeauth() const { return screen_ == Screen::Deauth; }
    const model::DeauthModel &deauthModel() const { return deauth_; }
    size_t deauthCursor() const { return deauth_cursor_; }
    bool showingAntiSurveillance() const { return screen_ == Screen::AntiSurveillance; }
    const model::AntiSurveillanceModel &antiModel() const { return anti_; }
    size_t antiCursor() const { return anti_cursor_; }
    bool antiStarting() const { return anti_start_pending_; }
    bool showingSettings() const { return screen_ == Screen::Settings; }
    const model::ContactsModel &contactsModel() const { return contacts_; }
    size_t contactsCursor() const { return contacts_cursor_; }
    bool contactsProbes() const { return contacts_tab_ == ui::ContactsTab::Probes; }
    const model::ContactInspection &contactInspection() const { return contact_inspection_; }
    bool showingContacts() const { return screen_ == Screen::Contacts; }
    const model::ZigModel &zigModel() const { return zig_; }
    size_t zigCursor() const { return zig_cursor_; }
    bool zigPans() const { return zig_tab_ == ui::ZigTab::Pans; }
    bool showingZig() const { return screen_ == Screen::Zig; }
    bool showingSystem() const { return screen_ == Screen::Link || screen_ == Screen::Info; }
    bool showingInfo() const { return screen_ == Screen::Info; }
    model::SystemStatus externalSystemState() const;
    const model::BtModel &btModel() const { return bt_; }
    size_t btCursor() const { return bt_cursor_; }
    bool showingBt() const { return screen_ == Screen::Beacons; }
    bool showingLora() const { return screen_ == Screen::SubGhz; }
    uint32_t gnssChecksumFailures() const { return gnss_parser_.checksumFailures(); }
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
    /* Wi-Fi region (OCP-SPEC §10.8) and band scope (§10.9). main.cpp passes
     * saved values at boot; unknown/empty becomes the default. */
    void setWifiRegion(const std::string &code) { wifi_region_ = app::regionOrDefault(code).code; }
    void setWifiBand(const std::string &code) { wifi_band_ = app::bandOrDefault(code); }

    /* True when the screen should be redrawn. */
    bool dirty(uint32_t now_ms) const;
    void draw(uint32_t now_ms);

    const ocp::Client &client() const { return client_; }

    /* Diagnostics sink. Counts and states only: never SSIDs or BSSIDs. */
    void onLog(std::function<void(const std::string &)> log) { log_ = std::move(log); }

private:
    model::SessionReview session_review_;
    bool session_previous_page_=false,system_from_sessions_=false;
    void openSessions(uint32_t now);
    void sessionKeys(char c);
    void sessionEnter();
    void tickSessions(uint32_t now);
    std::string debugSnapshot(uint32_t now_ms)const;
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
    void clearPhyBelief();
    void clearLoraBelief();
    void stopPhy(uint32_t now_ms);
    void stopPhyForNavigation(uint32_t now_ms);
    void toggleWifiContinuous(uint32_t now_ms);
    void startInspect(uint32_t now_ms);
    void openWifiDossier();void openBtDossier();
    void refreshWifiDossier(uint32_t now);void refreshBtDossier(uint32_t now);
    uint16_t absorbWifiPage(const ocp::Item &item);
    void absorbBtSnapshot(const ocp::Item &item);
    void absorbDeviceEvent(const ocp::Item &item);
    model::WifiInspection wifi_inspection_;
    model::BtInspection bt_inspection_;
    std::string ap_focus_bssid_,bt_focus_mac_;
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
    bool wardriveRowsPending() const;
    void tickWardrive(uint32_t now);
    void finishWardrive();
    void gnssDiagnostic(const std::string &arg,uint32_t now);
    model::GnssNoise gnss_noise_;
    bool wardrive_rescan_=false,wardrive_closing_=false;
    model::GnssFix wardrive_page_fix_;
    uint32_t wardrive_page_fix_ms_=0,scan_wait_started_ms_=0;
    void back(uint32_t now_ms);
    void openContactInspection();
    void absorbContactSnapshot(const ocp::Item &item);
    model::ContactInspection contact_inspection_;
    void navigateCard(Screen target,uint32_t now_ms);
    SystemNavigation system_navigation_;
    void notice(const std::string &text);
    /* ';'/'.' on the Settings screen: clamps to [kBrightnessMin,
     * kBrightnessMax] (never 0 — a black screen has no way back), applies
     * live via M5Cardputer.Display.setBrightness(), and persists. */
    void adjustBrightness(int direction);
    /* Settings `,` `/`: next/previous region, saved to SD and pushed to the
     * probe once its PHY lane is idle (pushWifiRegionIfDue). */
    void changeWifiRegion(const std::string &code);
    void pushWifiRegionIfDue(uint32_t now_ms);
    /* Settings `b`: cycle Wi-Fi band scope (auto/24/5), saved to SD and pushed
     * once the PHY lane is idle. `channels` debug command forwards directly. */
    void changeWifiBand(const std::string &code);
    void pushWifiBandIfDue(uint32_t now_ms);
    void sendWifiChannels(const std::string &arg, uint32_t now_ms);

    /* A command couldn't even be sent yet (client_.pending() from something
     * else in flight, not a real conflict) - retry it once that clears,
     * same idea as the scan-paging retry in tick(), generalized. Bounded so
     * a stuck link degrades to an error, not a silent forever-retry. */
    void retrySoon(std::function<void(uint32_t)> action, uint32_t now_ms);

    ocp::Client client_;
    model::ScanModel scan_;
    model::ContactsModel contacts_;
    model::SpectrumModel spectrum_;
    model::ScopeInspection scope_inspection_;
    model::PacketInspection packet_inspection_;
    model::LoraModel lora_;
    model::DeauthModel deauth_;
    model::BtModel bt_;
    model::AntiSurveillanceModel anti_;
    model::ZigModel zig_;
    gnss::NmeaParser gnss_parser_;
    model::GnssModel gnss_;
    model::DriveTrail drive_trail_;
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
    bool reconcile_on_status_ = false;

    /* Queued because the client couldn't send yet (something else was
     * still pending) - retried once that clears, see retrySoon(). */
    std::function<void(uint32_t)> pending_retry_;
    uint32_t pending_retry_started_ms_ = 0;

    std::string probe_owner_, probe_lora_;
    std::string wifi_region_ = OCP_WIFI_REGION_DEFAULT;   /* the deck's setting */
    std::string probe_region_;     /* what the probe last reported; empty: unknown/unsupported */
    bool region_push_pending_ = false;
    bool region_push_attempted_ = false;
    bool region_notice_pending_ = false;   /* user changed it: say when it lands */
    uint32_t region_push_ms_ = 0;
    std::string wifi_band_ = OCP_BAND_AUTO;
    std::string probe_band_;       /* empty when probe predates set_band */
    bool band_push_pending_ = false;
    bool band_push_attempted_ = false;
    bool band_notice_pending_ = false;
    uint32_t band_push_ms_ = 0;
    uint32_t probe_status_sequence_ = 0, probe_status_epoch_ = 0;
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
