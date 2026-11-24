/* Landscape GPS and SD activity view; Rev D §5–6. */
#include "ui/external_gps_display.h"

#include <Arduino.h>
#include <M5GFX.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "storage/sd_storage.h"
#include "storage/sd_activity.h"
#include "ui/external_spectrum_view.h"
#include "ui/external_wifi_view.h"
#include "ui/external_card_views.h"
#include "ui/external_hunt_view.h"
#include "ui/external_row_cache.h"
#include "ui/frame_snapshot.h"
#include "ui/external_lora_view.h"
#include "ui/external_drive_view.h"
#include "ui/external_bt_view.h"
#include "ui/wifi_detail_view.h"
#include "ui/bt_detail_view.h"
#include "ui/session_view.h"
#include "ui/external_contacts_view.h"
#include "ui/external_contact_detail_view.h"
#include "ui/external_zig_view.h"
#include "ui/external_ook_view.h"
#include "ui/theme.h"
#include "ui/external_tft.h"
#include "ui/external_gnss_view.h"

namespace ui {
namespace {
constexpr int kWidth = 320, kStripHeight = 24, kStrips = 10;
constexpr uint64_t kRowHashOffset = 14695981039346656037ULL;
constexpr uint64_t kRowHashPrime = 1099511628211ULL;
bool frame_resources_enabled=false;
model::SessionReview frame_sessions;
bool frame_sessions_enabled=false;
enum class ExternalView : uint8_t {
    Default,
    Link,
    Info,
    Wifi,
    ZigPans,
    ZigNodes,
    ContactsAccessPoints,
    ContactsProbes,
    ContactDetail,
    Bluetooth,
    BtDetail,
    TraceClients,
    SessionCatalog,SessionSummary,SessionRecords,SessionHex,
    Drive,
    Lora,
    LoraDetail,
    LoraSweep,
    Spectrum,
    Trace,
    Deauth,
    AntiSurveillance,
    SignalHunt,
    Settings,
    Ook,
};
constexpr uint32_t kDataFrameMs = 50;
constexpr uint32_t kStatusFrameMs = 250;
lgfx::LGFX_Sprite strip;
uint16_t row[kWidth];
detail::ExternalRowCache row_cache;
ExternalRenderStats render_stats;
uint32_t frame_started_us, last_model_sample;
uint32_t last_frame, started_at;
bool frame_startup, frame_system_enabled, frame_zig_enabled, frame_pans;
ExternalView active_view;
ExternalView frame_view;
size_t frame_zig_cursor;
model::ZigModel frame_zig;
model::SystemStatus frame_system;
model::SystemResources frame_resources;
model::WifiInspection frame_wifi_detail;
model::SurveyState frame_wifi_context;
model::BtInspection frame_bt_detail;
bool frame_wifi_detail_enabled,frame_bt_detail_enabled;
model::DeauthModel frame_deauth;
model::AntiSurveillanceModel frame_anti;
bool frame_trace_page, frame_trace_listening, frame_deauth_enabled, frame_anti_enabled, frame_anti_starting, frame_settings;
bool frame_deauth_new;
size_t frame_deauth_cursor, frame_anti_cursor;
uint16_t previous_scan_rows;
uint32_t previous_deauth_count, scan_event_until, deauth_event_until;
SettingsViewState frame_settings_state;
WifiBaselineBanner frame_wifi_baseline;
int next_strip = kStrips;
bool enabled;
bool display_sleeping;
bool force_full_frame;
model::SpectrumModel frame_spectrum;
size_t frame_cursor;
model::ScanModel frame_scan;
bool frame_scan_enabled;
size_t frame_scan_cursor;
bool frame_monitor, frame_lora_enabled;
model::LoraModel frame_lora;
model::LoraSweepModel frame_lora_sweep;
bool frame_lora_sweep_enabled;
model::SignalHunt frame_signal_hunt;
bool frame_signal_hunt_enabled;
model::PacketInspection frame_packet;
model::ContactsModel frame_wifi_contacts;
bool frame_packet_enabled;
size_t frame_lora_cursor;
bool frame_drive_enabled,frame_drive_closing;
model::DriveTrail frame_drive;
model::ContactsModel frame_contacts;
model::ContactInspection frame_contact_detail;
bool frame_contact_detail_enabled;
bool frame_contacts_enabled, frame_probes;
size_t frame_contacts_cursor;
model::BtModel frame_bt;
bool frame_bt_enabled;
size_t frame_bt_cursor;
model::GnssModel frame_gps;
model::OokModel frame_ook;
bool frame_ook_enabled;
size_t frame_ook_cursor;
storage::WardriveLogStats frame_log;
uint32_t frame_time;

bool sendStrip(int index,bool force)
{
    constexpr int tiles=detail::ExternalRowCache::kTiles, width=detail::ExternalRowCache::kTileWidth;
    /* Pinned M5GFX stores this unrotated 16-bit sprite in swapped RGB565. */
    const auto *pixels=static_cast<const uint16_t *>(strip.getBuffer());
    for(int y=0;y<kStripHeight;++y) {
        const int screen_y=index*kStripHeight+y;
        uint64_t hashes[tiles];bool changed[tiles];
        for(int tile=0;tile<tiles;++tile) {
            uint64_t hash=kRowHashOffset;
            for(int x=tile*width;x<(tile+1)*width;++x) {
                const uint16_t raw=pixels[y*kWidth+x];
                row[x]=uint16_t((raw<<8)|(raw>>8));hash^=raw;hash*=kRowHashPrime;
            }
            hashes[tile]=hash;changed[tile]=force||row_cache.needsTileUpdate(screen_y,tile,hash);
        }
        detail::ExternalRowCache::Span spans[tiles];
        const size_t count=detail::ExternalRowCache::planSpans(changed,spans);
        for(size_t span=0;span<count;++span) {
            const auto &run=spans[span];
            if(!writeExternalTftSpan(screen_y,run.first*width,run.count*width,row+run.first*width))return false;
            for(size_t i=run.first;i<run.first+run.count;++i)row_cache.storeTile(screen_y,i,hashes[i]);
        }
    }
    return true;
}
}

void holdExternalGpsDisplayReset() { holdExternalTftReset(); }

bool beginExternalGpsDisplay()
{
    enabled = false;
    strip.setColorDepth(16);
    if (!strip.createSprite(kWidth, kStripHeight) || !beginExternalTft()) return false;
    strip.fillScreen(kVoidInk);
    const auto *pixels=static_cast<const uint16_t *>(strip.getBuffer());
    if(uint16_t((pixels[0]<<8)|(pixels[0]>>8))!=strip.readPixel(0,0))return false;
    started_at = millis();
    row_cache.invalidate();
    force_full_frame = true;
    last_frame = millis() - 1000;
    frame_startup = true;
    active_view = ExternalView::Default;
    frame_view = ExternalView::Default;
    frame_time = started_at;
    for (int y = 0; y < kStrips; ++y) {
        strip.fillScreen(TFT_BLACK);
        drawExternalStartupStrip(strip, y * kStripHeight);
        if(!sendStrip(y,true))return false;
    }
    row_cache.validate();
    next_strip = kStrips;
    enabled = true;
    display_sleeping = false;
    return true;
}

bool setExternalGpsDisplaySleep(bool sleep, uint32_t now_ms)
{
    if (!enabled) return false;
    if (!setExternalTftSleep(sleep)) return false;
    display_sleeping = sleep;
    if (!sleep) {
        row_cache.invalidate();
        force_full_frame = true;
        next_strip = kStrips;
        last_frame = now_ms;
    }
    return true;
}

void tickExternalGpsDisplay(const model::GnssModel &gps, uint32_t bytes,
                            uint32_t checksum_failures, uint32_t now_ms,
                            const model::ScanModel *scan, size_t scan_cursor,
                            const model::SpectrumModel *spectrum, size_t cursor,
                            const model::LoraModel *lora, size_t lora_cursor, const model::DriveTrail *drive,
                            const model::BtModel *bt, size_t bt_cursor,
                            const model::ContactsModel *contacts, bool probes, size_t contacts_cursor,
                            const model::ZigModel *zig, bool pans, size_t zig_cursor,
                            const model::SystemStatus *system, const ExternalCardState *card)
{
    (void)bytes; (void)checksum_failures;
    if (!enabled || display_sleeping) return;
    const ExternalView view = card && card->signal_hunt ? ExternalView::SignalHunt :
                              card&&card->sessions ? (card->sessions->stage==model::ReviewStage::Catalog?ExternalView::SessionCatalog:card->sessions->stage==model::ReviewStage::Summary?ExternalView::SessionSummary:card->sessions->stage==model::ReviewStage::Records?ExternalView::SessionRecords:ExternalView::SessionHex) : system ? (system->info ? ExternalView::Info : ExternalView::Link) :
                              scan ? ExternalView::Wifi :
                              zig ? (pans ? ExternalView::ZigPans : ExternalView::ZigNodes) :
                              contacts ? (card && card->contact_detail ? ExternalView::ContactDetail : probes ? ExternalView::ContactsProbes : ExternalView::ContactsAccessPoints) :
                              bt ? (card && card->bt_detail ? ExternalView::BtDetail : ExternalView::Bluetooth) : drive ? ExternalView::Drive :
                              card && card->lora_sweep ? ExternalView::LoraSweep :
                              card && card->packet ? ExternalView::LoraDetail : lora ? ExternalView::Lora : spectrum ? ExternalView::Spectrum :
                              card && card->trace_page ? (card->wifi_detail && card->wifi_detail->clientsPage()?ExternalView::TraceClients:ExternalView::Trace) :
                              card && card->deauth ? ExternalView::Deauth :
                              card && card->anti ? ExternalView::AntiSurveillance :
                              card && card->settings ? ExternalView::Settings :
                              card && card->ook ? ExternalView::Ook : ExternalView::Default;
    if (view != active_view) {
        detail::discardFrameCopy(frame_scan);
        detail::discardFrameCopy(frame_wifi_contacts);
        detail::discardFrameCopy(frame_bt);
        detail::discardFrameCopy(frame_contacts);
        detail::discardFrameCopy(frame_sessions);
        detail::discardFrameCopy(frame_spectrum);
        detail::discardFrameCopy(frame_lora);
        detail::discardFrameCopy(frame_packet);
        detail::discardFrameCopy(frame_deauth);
        detail::discardFrameCopy(frame_anti);
        detail::discardFrameCopy(frame_zig);
        detail::discardFrameCopy(frame_wifi_detail);
        detail::discardFrameCopy(frame_bt_detail);
        detail::discardFrameCopy(frame_contact_detail);
        detail::discardFrameCopy(frame_system);
        detail::discardFrameCopy(frame_settings_state);
        detail::discardFrameCopy(frame_signal_hunt);
        detail::discardFrameCopy(frame_ook);
        active_view = view;
        row_cache.invalidate();
        force_full_frame = true;
        next_strip = kStrips;
        last_frame = now_ms - kStatusFrameMs;
    }
    if (next_strip == kStrips) {
        const bool data_view = scan || spectrum || lora || drive || bt || contacts || zig ||
                               (card && (card->trace_page || card->deauth || card->anti || card->lora_sweep || card->signal_hunt || card->ook));
        const uint32_t frame_ms = data_view ? kDataFrameMs : kStatusFrameMs;
        if (now_ms - last_frame < frame_ms) return;
        frame_started_us=micros();
        const bool resample=force_full_frame||now_ms-last_model_sample>=200||
            scan_cursor!=frame_scan_cursor||cursor!=frame_cursor||lora_cursor!=frame_lora_cursor||
            bt_cursor!=frame_bt_cursor||contacts_cursor!=frame_contacts_cursor||zig_cursor!=frame_zig_cursor;
        if(resample)last_model_sample=now_ms;
        frame_gps=gps;
        frame_sessions_enabled=card&&card->sessions;
        if(frame_sessions_enabled&&resample)frame_sessions=*card->sessions;
        frame_monitor = spectrum != nullptr;
        frame_view = view;
        frame_lora_enabled = lora != nullptr;
        frame_lora_sweep_enabled = card && card->lora_sweep;
        if (frame_lora_sweep_enabled) frame_lora_sweep = *card->lora_sweep;
        frame_signal_hunt_enabled = card && card->signal_hunt;
        if (frame_signal_hunt_enabled) frame_signal_hunt = *card->signal_hunt;
        frame_packet_enabled=card && card->packet;
        if(frame_packet_enabled)frame_packet=*card->packet;
        frame_startup=now_ms-started_at<2000;
        frame_scan_enabled=scan!=nullptr;
        if((scan || (card && card->wifi_detail)) && resample)frame_wifi_contacts=card && card->observed_contacts ? *card->observed_contacts : model::ContactsModel{};
        if(scan) {
            if(resample)frame_scan=*scan;
            const auto count=static_cast<uint16_t>(std::min<size_t>(scan->rows().size(), UINT16_MAX));
            if(view==ExternalView::Wifi && count>previous_scan_rows) scan_event_until=now_ms+700;
            previous_scan_rows=count;
        } else if(view!=ExternalView::Wifi) previous_scan_rows=0;
        frame_scan_cursor=scan_cursor;
        frame_wifi_baseline=card ? card->wifi_baseline : WifiBaselineBanner{};
        frame_system_enabled=system!=nullptr;
        if(system) {
            frame_system=*system;
            frame_resources_enabled=card && card->system_resources;
            if(frame_resources_enabled)frame_resources=*card->system_resources;
        }
        frame_wifi_detail_enabled=card && card->wifi_detail;
        if(frame_wifi_detail_enabled&&resample){frame_wifi_detail=*card->wifi_detail;if(card->wifi_context)frame_wifi_context=model::SurveyState(*card->wifi_context);}
        frame_bt_detail_enabled=card && card->bt_detail;
        if(frame_bt_detail_enabled&&resample)frame_bt_detail=*card->bt_detail;
        frame_trace_page=card && card->trace_page;
        frame_trace_listening=card && card->trace_listening;
        frame_deauth_enabled=card && card->deauth;
        if(frame_deauth_enabled) {
            frame_deauth=*card->deauth;
            if(view==ExternalView::Deauth && frame_deauth.totalCount()>previous_deauth_count) deauth_event_until=now_ms+700;
            previous_deauth_count=frame_deauth.totalCount();
        } else if(view!=ExternalView::Deauth) previous_deauth_count=0;
        frame_deauth_new=now_ms<deauth_event_until;
        frame_deauth_cursor=card ? card->deauth_cursor : 0;
        frame_anti_enabled=card && card->anti;
        if(frame_anti_enabled) frame_anti=*card->anti;
        frame_anti_cursor=card ? card->anti_cursor : 0;
        frame_anti_starting=card && card->anti_starting;
        frame_ook_enabled=card && card->ook;
        if(frame_ook_enabled&&resample) frame_ook=*card->ook;
        frame_ook_cursor=card ? card->ook_cursor : 0;
        frame_settings=card && card->settings;
        if(frame_settings) frame_settings_state=*card->settings;
        frame_zig_enabled=zig!=nullptr;
        if(zig) frame_zig=*zig;
        frame_pans=pans;frame_zig_cursor=zig_cursor;
        frame_contacts_enabled = contacts != nullptr;
        frame_contact_detail_enabled=card && card->contact_detail;
        if(frame_contact_detail_enabled && resample)frame_contact_detail=*card->contact_detail;
        if(contacts&&resample) frame_contacts=*contacts;
        frame_probes=probes;
        frame_contacts_cursor=contacts_cursor;
        frame_bt_enabled = bt != nullptr;
        if(bt&&resample) frame_bt=*bt;
        frame_bt_cursor=bt_cursor;
        frame_drive_enabled = drive != nullptr;
        frame_drive_closing=card && card->drive_closing;
        if(drive) {
            frame_drive=*drive;frame_gps=gps;
#if !defined(OSCILLA_SD_DISPLAY_BENCH)
            frame_log=storage::wardriveLogStats();
#endif
        }
        if (lora) frame_lora = *lora;
        frame_lora_cursor = lora_cursor;
        if (spectrum) frame_spectrum = *spectrum;
        frame_cursor = cursor;
        frame_time = now_ms;
        last_frame = now_ms;
        next_strip = 0;
    }
    strip.fillScreen(TFT_BLACK);
    if(frame_startup && frame_view == ExternalView::Default) {
        drawExternalStartupStrip(strip,next_strip*kStripHeight);
    } else if (frame_signal_hunt_enabled) {
        drawExternalHuntStrip(strip, next_strip * kStripHeight, frame_signal_hunt, frame_time);
    } else if(frame_system_enabled) {
        drawExternalSystemStrip(strip,next_strip*kStripHeight,frame_system,frame_time,frame_resources_enabled?&frame_resources:nullptr);
    } else if(frame_scan_enabled) {
        drawExternalWifiStrip(strip,next_strip*kStripHeight,frame_scan,frame_scan_cursor,frame_time,frame_time<scan_event_until,&frame_wifi_contacts,frame_wifi_baseline.active?&frame_wifi_baseline:nullptr);
    } else if(frame_sessions_enabled) {
        drawExternalSessionStrip(strip,next_strip*kStripHeight,frame_sessions);
    } else if(frame_trace_page) {
        drawExternalWifiDetailStrip(strip,next_strip*kStripHeight,frame_wifi_detail,frame_wifi_context,frame_wifi_contacts,frame_trace_listening,frame_time);
    } else if(frame_deauth_enabled) {
        drawExternalDeauthStrip(strip,next_strip*kStripHeight,frame_deauth,frame_deauth_cursor,frame_deauth_new,frame_time);
    } else if(frame_anti_enabled) {
        drawExternalAntiStrip(strip,next_strip*kStripHeight,frame_anti,frame_anti_cursor,frame_anti_starting,frame_time);
    } else if(frame_ook_enabled) {
        drawExternalOokStrip(strip,next_strip*kStripHeight,frame_ook,frame_ook_cursor,frame_time);
    } else if(frame_settings) {
        drawExternalSettingsStrip(strip,next_strip*kStripHeight,frame_settings_state);
    } else if(frame_zig_enabled) {
        drawExternalZigStrip(strip,next_strip*kStripHeight,frame_zig,frame_pans,frame_zig_cursor);
    } else if(frame_contacts_enabled) {
        if(frame_contact_detail_enabled)drawExternalContactDetailStrip(strip,next_strip*kStripHeight,frame_contacts,frame_contact_detail,frame_time);
        else drawExternalContactsStrip(strip,next_strip*kStripHeight,frame_contacts,frame_probes,frame_contacts_cursor,frame_time);
    } else if(frame_bt_enabled) {
        if(frame_bt_detail_enabled)drawExternalBtDetailStrip(strip,next_strip*kStripHeight,frame_bt_detail,frame_bt,frame_time);
        else drawExternalBtStrip(strip,next_strip*kStripHeight,frame_bt,frame_bt_cursor);
    } else if(frame_drive_enabled) {
        drawExternalDriveStrip(strip,next_strip*kStripHeight,frame_gps,frame_drive,frame_log,frame_time,frame_drive_closing);
    } else if (frame_lora_sweep_enabled) {
        drawExternalLoraSweepStrip(strip, next_strip * kStripHeight, frame_lora_sweep, frame_time);
    } else if (frame_lora_enabled) {
        if(frame_packet_enabled)drawExternalLoraDetailStrip(strip,next_strip*kStripHeight,frame_packet);
        else drawExternalLoraStrip(strip, next_strip * kStripHeight, frame_lora, frame_lora_cursor, frame_time);
    } else if (frame_monitor) {
        strip.fillScreen(kVoidInk);
        drawExternalSpectrumStrip(strip, next_strip * kStripHeight, frame_spectrum, frame_cursor, frame_time);
    }
    else {
        drawExternalGnssStrip(strip,next_strip*kStripHeight,frame_gps,frame_time);
    }
#if defined(OSCILLA_SD_DISPLAY_BENCH)
    strip.setTextSize(1);strip.setTextColor(kCalibrationYellow,kVoidInk);
    strip.setCursor(12,10-next_strip*kStripHeight);strip.print("SYNTHETIC / SD BUS TEST");
#endif
    if(!sendStrip(next_strip,force_full_frame))return;
    ++next_strip;
    if (next_strip >= kStrips) {
        force_full_frame = false;
        row_cache.validate();
        ++render_stats.frames;
        render_stats.last_frame_us=micros()-frame_started_us;
        render_stats.max_frame_us=std::max(render_stats.max_frame_us,render_stats.last_frame_us);
    }
}
const ExternalRenderStats &externalRenderStats() { return render_stats; }

}
