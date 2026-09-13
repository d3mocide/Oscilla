/*
 * deck_app.h — screen flow and command orchestration for the deck.
 *
 *   Link <-,/-> Contacts <-,/-> Info <-,/-> Spectrum <-,/-> SubGhz <-,/-> Deauth <-,/-> (wraps)
 *   Link --w--> Sweep --enter--> Trace                               drill-down
 *   Spectrum --enter--> (locks to one channel, same screen)
 *   ` = stop + back (DESIGN §7.3)
 *
 * Deauth has no DESIGN §7.2 view of its own yet — added ahead of the UI
 * rework DESIGN will eventually need for a growing card set (deliberate,
 * not an oversight: see WORKLOG).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "model/contacts_model.h"
#include "model/deauth_model.h"
#include "model/lora_model.h"
#include "model/scan_model.h"
#include "model/spectrum_model.h"
#include "ocp/ocp_client.h"
#include "ui/contacts_view.h"

namespace app {

enum class Screen : uint8_t { Link, Sweep, Trace, Contacts, Info, Spectrum, SubGhz, Deauth };

struct Keys {
    std::string chars;   /* printable keys pressed this frame */
    bool enter = false;
};

class DeckApp {
public:
    explicit DeckApp(ocp::Client::Write write);

    void begin(uint32_t now_ms);
    void feed(const uint8_t *data, size_t len, uint32_t now_ms) { client_.feed(data, len, now_ms); }
    void tick(uint32_t now_ms);
    void onKeys(const Keys &keys, uint32_t now_ms);

    /* True when the screen should be redrawn. */
    bool dirty(uint32_t now_ms) const;
    void draw(uint32_t now_ms);

    const ocp::Client &client() const { return client_; }

    /* Diagnostics sink. Counts and states only: never SSIDs or BSSIDs. */
    void onLog(std::function<void(const std::string &)> log) { log_ = std::move(log); }

private:
    void onReply(const ocp::Item &it);
    void onEvent(const ocp::Item &it);
    void startScan(uint32_t now_ms);
    void startInspect(uint32_t now_ms);
    void startSniffer(uint32_t now_ms);
    void startChannelView(uint32_t now_ms);
    void startPacketMonitor(uint32_t now_ms, uint8_t ch);
    void startLoraConfig(uint32_t now_ms);
    void startLoraListen(uint32_t now_ms);
    void startDeauthDetector(uint32_t now_ms);
    void back(uint32_t now_ms);
    void notice(const std::string &text);

    ocp::Client client_;
    model::ScanModel scan_;
    model::ContactsModel contacts_;
    model::SpectrumModel spectrum_;
    model::LoraModel lora_;
    model::DeauthModel deauth_;
    Screen screen_ = Screen::Link;
    size_t cursor_ = 0;
    uint16_t trace_idx_ = 0;
    size_t contacts_cursor_ = 0;
    ui::ContactsTab contacts_tab_ = ui::ContactsTab::Clients;
    bool contacts_poll_clients_ = true;
    uint32_t last_contacts_poll_ms_ = 0;
    size_t spectrum_cursor_ = 0;
    size_t lora_cursor_ = 0;
    size_t deauth_cursor_ = 0;

    bool probe_status_valid_ = false;
    uint32_t probe_heap_ = 0;
    uint64_t probe_uptime_ms_ = 0;
    uint32_t last_status_reply_ms_ = 0;
    uint32_t last_status_poll_ms_ = 0;

    std::string last_reply_ = "-";
    std::string notice_;
    uint16_t next_page_ = 0;
    uint32_t scan_started_ms_ = 0;
    uint32_t last_draw_ms_ = 0;
    uint32_t last_attempt_ms_ = 0;
    uint32_t last_keepalive_ms_ = 0;
    uint32_t now_ = 0;
    bool dirty_ = true;
    std::function<void(const std::string &)> log_;
    void log(const std::string &line) { if (log_) log_(line); }
};

}  // namespace app
