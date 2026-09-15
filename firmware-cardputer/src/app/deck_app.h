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
 * Cycling cards (`,`/`/`) deliberately does *not* stop the screen being
 * left — Wi-Fi and LoRa are separate radios (radio_arbiter only ever
 * tracks Wi-Fi/BLE/802.15.4) and running both at once (e.g. wardriving
 * Wi-Fi while LoRa listens) is a real, supported use, not an oversight.
 * Two consequences of keeping engines running in the background:
 *   - A command can arrive while an unrelated engine is still streaming
 *     events over the same Grove UART, so its own reply may simply be
 *     delayed rather than lost. Every start*() queues itself via
 *     retrySoon() if client_.send() couldn't go out yet (something else
 *     was pending) rather than just failing — see WORKLOG 2026-09-14.
 *   - Two Wi-Fi-family engines genuinely cannot run at once (one radio),
 *     so a start*() that arrives while another already owns the arbiter
 *     gets OCP_ERR_BUSY ("radio in use by ..."). Since pressing that
 *     start key already signals a deliberate switch, that specific error
 *     triggers an immediate stop-then-retry handoff instead of just an
 *     error notice (armed_busy_retry_/handoff_retry_, same WORKLOG entry).
 *     LoRa never participates in this — it has no arbiter conflict to
 *     hand off from.
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
    void feed(const uint8_t *data, size_t len, uint32_t now_ms) { now_ = now_ms; client_.feed(data, len, now_ms); }
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
    Screen screen_ = Screen::Link;
    size_t cursor_ = 0;
    uint16_t trace_idx_ = 0;
    size_t contacts_cursor_ = 0;
    ui::ContactsTab contacts_tab_ = ui::ContactsTab::Clients;
    bool contacts_poll_clients_ = true;
    uint32_t last_contacts_poll_ms_ = 0;
    size_t spectrum_cursor_ = 0;
    size_t lora_cursor_ = 0;
    /* A rejected lora_listen (e.g. sent before lora_config) must not create
     * a session file — the ack only exists after the probe actually
     * confirms, so the [LORA]/error reply is what triggers lora_.begin()
     * and storage::loraLogBegin(), not the optimistic send. */
    bool lora_listen_pending_ = false;
    size_t deauth_cursor_ = 0;

    /* Queued because the client couldn't send yet (something else was
     * still pending) - retried once that clears, see retrySoon(). */
    std::function<void(uint32_t)> pending_retry_;
    uint32_t pending_retry_started_ms_ = 0;
    /* What to do if the command currently in flight comes back
     * OCP_ERR_BUSY from radio_arbiter ("radio in use by <owner>") - a real
     * same-PHY-lane conflict (two Wi-Fi-family engines), not the queuing
     * artifact pending_retry_ handles. Set only by the arbiter-gated
     * start*() calls; always cleared once that command's own reply lands,
     * one way or another (see onReply()/tick()) so it can never fire for
     * an unrelated later error. */
    std::function<void(uint32_t)> armed_busy_retry_;
    /* Set from armed_busy_retry_ once a handoff is underway: the auto-sent
     * `stop` is in flight, and this runs when its [STOP] lands. */
    std::function<void(uint32_t)> handoff_retry_;

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
